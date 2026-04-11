#include "core/AdaptiveBrightness.h"

#include <Arduino.h>
#include <cmath>

#ifdef VIRTUAL_PANE
#include <ESP32-VirtualMatrixPanel-I2S-DMA.h>
#else
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#endif

#include "debug_config.h"
#include "deviceConfig.h"

extern bool autoBrightnessEnabled;
extern uint8_t userBrightness;
extern void updateGlobalBrightnessScale(uint8_t brightness);

namespace
{

bool apdsInitialized = false;
bool apdsFaulted = false;
unsigned long lastApdsProximityCheck = 0;
unsigned long lastApdsInitRetry = 0;
unsigned long lastLuxUpdate = 0;

constexpr unsigned long APDS_PROX_FALLBACK_MS = 750;
constexpr unsigned long APDS_PROX_CACHE_MS = 50;
constexpr unsigned long APDS_INIT_RETRY_MS = 2000;

uint16_t lastKnownClearValue = 0;
uint16_t lastKnownRedValue = 0;
uint16_t lastKnownGreenValue = 0;
uint16_t lastKnownBlueValue = 0;
float lastKnownLuxValue = 0.0f;
apds9960AGain_t apdsColorGain = APDS9960_AGAIN_4X;
uint16_t apdsIntegrationTimeMs = 10;
constexpr uint16_t APDS_LUX_INTEGRATION_MIN_MS = 3;
constexpr uint16_t APDS_LUX_INTEGRATION_MAX_MS = 100;
constexpr uint16_t APDS_CLEAR_LOW_COUNT = 200;
constexpr uint16_t APDS_CLEAR_HIGH_COUNT = 8000;
constexpr float APDS_LUX_REFERENCE_INTEGRATION_MS = 10.0f;
constexpr float APDS_LUX_REFERENCE_GAIN = 4.0f;

float smoothedLux = 50.0f;
constexpr float luxSmoothingFactor = 0.35f;
float currentBrightness = 128.0f;
constexpr float brightnessSmoothingFactor = 0.95f;
constexpr float manualBrightnessSmoothingFactor = 0.04f;
int lastBrightness = 0;
constexpr int brightnessThreshold = 1;
constexpr unsigned long luxUpdateInterval = 250;

#if defined(APDS_AVAILABLE)
volatile bool apdsProximityInterruptFlag = false;

void IRAM_ATTR onApdsInterrupt()
{
  apdsProximityInterruptFlag = true;
}
#endif

float apdsGainToFloat(apds9960AGain_t gain)
{
  switch (gain)
  {
  case APDS9960_AGAIN_1X:
    return 1.0f;
  case APDS9960_AGAIN_4X:
    return 4.0f;
  case APDS9960_AGAIN_16X:
    return 16.0f;
  case APDS9960_AGAIN_64X:
    return 64.0f;
  default:
    return 4.0f;
  }
}

apds9960AGain_t apdsStepGain(apds9960AGain_t gain, bool increase)
{
  switch (gain)
  {
  case APDS9960_AGAIN_1X:
    return increase ? APDS9960_AGAIN_4X : APDS9960_AGAIN_1X;
  case APDS9960_AGAIN_4X:
    return increase ? APDS9960_AGAIN_16X : APDS9960_AGAIN_1X;
  case APDS9960_AGAIN_16X:
    return increase ? APDS9960_AGAIN_64X : APDS9960_AGAIN_4X;
  case APDS9960_AGAIN_64X:
    return increase ? APDS9960_AGAIN_64X : APDS9960_AGAIN_16X;
  default:
    return APDS9960_AGAIN_4X;
  }
}

float calculateLuxFromRgb(uint16_t r, uint16_t g, uint16_t b)
{
  float illuminance = (-0.32466f * r) + (1.57837f * g) + (-0.73191f * b);
  if (illuminance < 0.0f)
  {
    illuminance = 0.0f;
  }
  return illuminance;
}

float scaleLuxForSettings(float luxRaw)
{
  const float gain = apdsGainToFloat(apdsColorGain);
  const float integrationMs = static_cast<float>(apdsIntegrationTimeMs);
  if (gain <= 0.0f || integrationMs <= 0.0f)
  {
    return luxRaw;
  }
  const float scale = (APDS_LUX_REFERENCE_INTEGRATION_MS * APDS_LUX_REFERENCE_GAIN) / (integrationMs * gain);
  return luxRaw * scale;
}

void adjustApdsColorRange(uint16_t clear)
{
  bool changed = false;

  if (clear >= APDS_CLEAR_HIGH_COUNT)
  {
    if (apdsColorGain != APDS9960_AGAIN_1X)
    {
      apdsColorGain = apdsStepGain(apdsColorGain, false);
      changed = true;
    }
    else if (apdsIntegrationTimeMs > APDS_LUX_INTEGRATION_MIN_MS)
    {
      apdsIntegrationTimeMs = static_cast<uint16_t>(apdsIntegrationTimeMs / 2);
      if (apdsIntegrationTimeMs < APDS_LUX_INTEGRATION_MIN_MS)
      {
        apdsIntegrationTimeMs = APDS_LUX_INTEGRATION_MIN_MS;
      }
      changed = true;
    }
  }
  else if (clear <= APDS_CLEAR_LOW_COUNT)
  {
    if (apdsColorGain != APDS9960_AGAIN_64X)
    {
      apdsColorGain = apdsStepGain(apdsColorGain, true);
      changed = true;
    }
    else if (apdsIntegrationTimeMs < APDS_LUX_INTEGRATION_MAX_MS)
    {
      apdsIntegrationTimeMs = static_cast<uint16_t>(apdsIntegrationTimeMs * 2);
      if (apdsIntegrationTimeMs > APDS_LUX_INTEGRATION_MAX_MS)
      {
        apdsIntegrationTimeMs = APDS_LUX_INTEGRATION_MAX_MS;
      }
      changed = true;
    }
  }

  if (changed)
  {
    apds.setADCGain(apdsColorGain);
    apds.setADCIntegrationTime(apdsIntegrationTimeMs);
#if DEBUG_BRIGHTNESS
    Serial.printf("APDS range adjust: gain=%.0fx it=%ums\n",
                  apdsGainToFloat(apdsColorGain),
                  apdsIntegrationTimeMs);
#endif
  }
}

void updateAmbientLightSample()
{
  static unsigned long lastLogTime = 0;
  static unsigned long lastForcedReadTime = 0;
  const unsigned long logInterval = 500;

  if (!apdsInitialized || apdsFaulted)
  {
    return;
  }

  const unsigned long now = millis();
  const bool ready = apds.colorDataReady();
  const bool forceRead = (now - lastForcedReadTime) >= APDS_PROX_FALLBACK_MS;
  if (ready || forceRead)
  {
    uint16_t r, g, b, c;
    apds.getColorData(&r, &g, &b, &c);
    lastForcedReadTime = now;

    lastKnownRedValue = r;
    lastKnownGreenValue = g;
    lastKnownBlueValue = b;
    lastKnownClearValue = c;
    lastKnownLuxValue = scaleLuxForSettings(calculateLuxFromRgb(r, g, b));

    adjustApdsColorRange(c);

#if DEBUG_BRIGHTNESS
    if (now - lastLogTime >= logInterval)
    {
      lastLogTime = now;
      Serial.printf("Ambient light: clear=%u lux=%.1f gain=%.0fx it=%ums\n",
                    c,
                    lastKnownLuxValue,
                    apdsGainToFloat(apdsColorGain),
                    apdsIntegrationTimeMs);
    }
#endif
  }
}

void updateAdaptiveBrightness()
{
  if (!autoBrightnessEnabled)
  {
    const float targetManualBrightness = static_cast<float>(userBrightness);
    currentBrightness += manualBrightnessSmoothingFactor * (targetManualBrightness - currentBrightness);
    int smoothedBrightness = static_cast<int>(currentBrightness + 0.5f);
    smoothedBrightness = constrain(smoothedBrightness, 0, 255);

    if (abs(smoothedBrightness - lastBrightness) >= brightnessThreshold)
    {
      const uint8_t brightnessToApply = static_cast<uint8_t>(smoothedBrightness);
      dma_display->setBrightness8(brightnessToApply);
      updateGlobalBrightnessScale(brightnessToApply);
      lastBrightness = smoothedBrightness;
#if DEBUG_BRIGHTNESS
      Serial.printf(">>>> MANUAL: BRIGHTNESS SET TO %d (target=%u) <<<<\n", smoothedBrightness, userBrightness);
#endif
    }
    return;
  }

  if (!apdsInitialized || apdsFaulted)
  {
    const unsigned long now = millis();
    if ((now - lastApdsInitRetry) >= APDS_INIT_RETRY_MS)
    {
      lastApdsInitRetry = now;
      apdsFaulted = false;
      setupAdaptiveBrightness();
    }
    if (lastBrightness != userBrightness)
    {
      dma_display->setBrightness8(userBrightness);
      updateGlobalBrightnessScale(userBrightness);
      lastBrightness = userBrightness;
    }
    currentBrightness = static_cast<float>(userBrightness);
    return;
  }

  static float cachedLux = 0.0f;
  static bool hasCachedLux = false;
  const unsigned long now = millis();

  if (!hasCachedLux || (now - lastLuxUpdate) >= luxUpdateInterval)
  {
    cachedLux = getAmbientLux();
    smoothedLux = (luxSmoothingFactor * cachedLux) + ((1.0f - luxSmoothingFactor) * smoothedLux);
    lastLuxUpdate = now;
    hasCachedLux = true;
  }

  const float currentLux = cachedLux;
  const int min_brightness_output = 80;
  const int max_brightness_output = 255;
  const float min_lux_for_map = 5.0f;
  const float max_lux_for_map = 10000.0f;

  float luxForMap = smoothedLux;
  if (luxForMap < min_lux_for_map)
  {
    luxForMap = min_lux_for_map;
  }

  const float logMin = log10f(min_lux_for_map);
  const float logMax = log10f(max_lux_for_map);
  float t = (log10f(luxForMap) - logMin) / (logMax - logMin);
  t = constrain(t, 0.0f, 1.0f);

  int targetBrightnessCalc = static_cast<int>(
      min_brightness_output + (t * static_cast<float>(max_brightness_output - min_brightness_output)) + 0.5f);
  targetBrightnessCalc = constrain(targetBrightnessCalc, min_brightness_output, max_brightness_output);

  const float targetBrightness = static_cast<float>(targetBrightnessCalc);
  currentBrightness += brightnessSmoothingFactor * (targetBrightness - currentBrightness);
  int smoothedBrightness = static_cast<int>(currentBrightness + 0.5f);
  smoothedBrightness = constrain(smoothedBrightness, min_brightness_output, max_brightness_output);

#if DEBUG_BRIGHTNESS
  Serial.printf("ADAPT: Clear=%u, Lux=%.1f, SmoothLux=%.1f, TargetBr=%d, SmoothBr=%d, LastBr=%d, Thr=%d\n",
                lastKnownClearValue,
                currentLux,
                smoothedLux,
                targetBrightnessCalc,
                smoothedBrightness,
                lastBrightness,
                brightnessThreshold);
#endif

  if (abs(smoothedBrightness - lastBrightness) >= brightnessThreshold)
  {
    const uint8_t brightnessToApply = static_cast<uint8_t>(smoothedBrightness);
    dma_display->setBrightness8(brightnessToApply);
    updateGlobalBrightnessScale(brightnessToApply);
    lastBrightness = smoothedBrightness;
#if DEBUG_BRIGHTNESS
    Serial.printf(">>>> ADAPT: BRIGHTNESS SET TO %d <<<<\n", smoothedBrightness);
#endif
  }
  else
  {
#if DEBUG_BRIGHTNESS
    Serial.printf(">>>> ADAPT: BRIGHTNESS KEPT AT %d <<<<\n", lastBrightness);
#endif
  }
}

} // namespace

void setupAdaptiveBrightness()
{
#if defined(APDS_AVAILABLE)
  Wire.begin(APDS_SDA_PIN, APDS_SCL_PIN);
  Wire.setClock(400000);
  LOG_PROX_LN("APDS init: starting Wire and begin()");

  if (!apds.begin())
  {
#if DEBUG_MODE
    Serial.println("failed to initialize proximity sensor! Please check your wiring.");
#endif
    LOG_PROX_LN("APDS init: begin() failed");
    apdsFaulted = true;
    apdsInitialized = false;
    return;
  }

  apdsInitialized = true;
  apdsFaulted = false;
#if DEBUG_MODE
  Serial.println("Proximity sensor initialized!");
#endif
  LOG_PROX_LN("APDS init: begin() succeeded");
  apds.setProxGain(APDS9960_PGAIN_4X);
  apds.setADCGain(apdsColorGain);
  apds.setADCIntegrationTime(apdsIntegrationTimeMs);
  apds.enableProximity(true);
  apds.enableColor(true);
  apds.setProximityInterruptThreshold(0, 175);
  apds.enableProximityInterrupt();
#ifdef APDS_INT_PIN
  pinMode(APDS_INT_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(APDS_INT_PIN), onApdsInterrupt, FALLING);
#endif
#else
  (void)apdsFaulted;
  (void)apdsInitialized;
#endif
}

uint16_t getRawClearChannelValue()
{
  updateAmbientLightSample();
  return lastKnownClearValue;
}

float getAmbientLux()
{
  updateAmbientLightSample();
  return lastKnownLuxValue;
}

uint16_t getAmbientLuxU16()
{
  float lux = getAmbientLux();
  if (lux < 0.0f)
  {
    lux = 0.0f;
  }
  if (lux > 65535.0f)
  {
    lux = 65535.0f;
  }
  return static_cast<uint16_t>(lux + 0.5f);
}

void maybeUpdateBrightness()
{
  updateAdaptiveBrightness();
}

bool shouldReadProximity(unsigned long now)
{
#if defined(APDS_AVAILABLE)
  if (!apdsInitialized && !apdsFaulted)
  {
    apdsInitialized = apds.begin();
    apdsFaulted = !apdsInitialized;
    if (apdsInitialized)
    {
      apds.enableProximity(true);
      apds.enableColor(true);
      apds.setProximityInterruptThreshold(0, 175);
      apds.enableProximityInterrupt();
      LOG_PROX_LN("APDS lazy init: begin() succeeded");
    }
    else
    {
      LOG_PROX_LN("APDS lazy init: begin() failed");
      apdsFaulted = true;
      apdsInitialized = false;
    }
  }

  if (!apdsInitialized || apdsFaulted)
  {
    LOG_PROX_LN("APDS not initialized or faulted; skipping proximity read");
    return false;
  }

  if ((now - lastApdsProximityCheck) < APDS_PROX_CACHE_MS)
  {
    return false;
  }

  lastApdsProximityCheck = now;
  return true;
#else
  (void)now;
  return false;
#endif
}

bool shouldDeferProximityRead(unsigned long now, unsigned long guardMs)
{
  if (!autoBrightnessEnabled)
  {
    return false;
  }
  return static_cast<unsigned long>(now - lastLuxUpdate) < guardMs;
}

void syncBrightnessState(uint8_t brightness)
{
  currentBrightness = static_cast<float>(brightness);
  lastBrightness = brightness;
}

int getLastAppliedBrightness()
{
  return lastBrightness;
}
