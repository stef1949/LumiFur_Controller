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
#include "perf_tuning.h"
#include "core/PerfTelemetry.h"

extern bool autoBrightnessEnabled;
extern uint8_t userBrightness;
extern void updateGlobalBrightnessScale(uint8_t brightness);

namespace
{

bool apdsInitialized = false;
bool apdsFaulted = false;
unsigned long lastApdsProximityCheck = 0;
unsigned long lastApdsInitRetry = 0;
unsigned long lastAmbientSampleMs = 0;
unsigned long lastProximitySampleMs = 0;

constexpr unsigned long APDS_INIT_RETRY_MS = 2000;

uint16_t lastKnownClearValue = 0;
uint8_t lastKnownProximityValue = 0;
bool lastKnownProximityValid = false;
bool lastKnownAmbientValid = false;
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
bool apdsInterruptConfigured = false;

#if defined(APDS_AVAILABLE) && defined(APDS_INT_PIN)
volatile bool apdsProximityInterruptPending = false;

void IRAM_ATTR onApdsInterrupt()
{
  apdsProximityInterruptPending = true;
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

#if defined(APDS_AVAILABLE)
bool readApdsRegisterBytes(uint8_t reg, uint8_t *buffer, size_t length)
{
  if (buffer == nullptr || length == 0)
  {
    return false;
  }

  Wire.beginTransmission(APDS9960_ADDRESS);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0)
  {
    return false;
  }

  const size_t bytesRead = Wire.requestFrom(static_cast<uint8_t>(APDS9960_ADDRESS),
                                            static_cast<uint8_t>(length),
                                            static_cast<uint8_t>(true));
  if (bytesRead != length)
  {
    while (Wire.available())
    {
      (void)Wire.read();
    }
    return false;
  }

  for (size_t i = 0; i < length; ++i)
  {
    if (!Wire.available())
    {
      return false;
    }
    buffer[i] = static_cast<uint8_t>(Wire.read());
  }
  return true;
}

bool readApdsClearChannel(uint16_t &clearOut)
{
  uint8_t raw[2] = {};
  if (!readApdsRegisterBytes(APDS9960_CDATAL, raw, sizeof(raw)))
  {
    return false;
  }

  clearOut = static_cast<uint16_t>(raw[0]) |
             (static_cast<uint16_t>(raw[1]) << 8);
  return true;
}
#endif

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
  const unsigned long logInterval = 500;

  if (!apdsInitialized || apdsFaulted)
  {
    return;
  }

  const unsigned long now = millis();
  if (lastKnownAmbientValid &&
      static_cast<unsigned long>(now - lastAmbientSampleMs) < LF_APDS_LUX_SAMPLE_INTERVAL_MS)
  {
    return;
  }

  const uint32_t startMicros = micros();
#if LF_APDS_USE_CLEAR_ONLY_LUX
  uint16_t c = 0;
  if (!readApdsClearChannel(c))
  {
    return;
  }

  lastKnownClearValue = c;
  lastKnownLuxValue = scaleLuxForSettings(static_cast<float>(c));
#else
  uint16_t r = 0;
  uint16_t g = 0;
  uint16_t b = 0;
  uint16_t c = 0;
    apds.getColorData(&r, &g, &b, &c);

    lastKnownClearValue = c;
    lastKnownLuxValue = scaleLuxForSettings(calculateLuxFromRgb(r, g, b));
#endif

  lastAmbientSampleMs = now;
  lastKnownAmbientValid = true;
  perfTelemetryRecordDuration(PerfDurationId::ApdsTransaction, micros() - startMicros);

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
  static unsigned long lastBrightnessProcessMs = 0;
  const unsigned long now = millis();

  if (!hasCachedLux || (now - lastBrightnessProcessMs) >= LF_APDS_LUX_SAMPLE_INTERVAL_MS)
  {
    cachedLux = getAmbientLux();
    smoothedLux = (luxSmoothingFactor * cachedLux) + ((1.0f - luxSmoothingFactor) * smoothedLux);
    lastBrightnessProcessMs = now;
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

void configureApdsRuntime()
{
  apds.setProxGain(APDS9960_PGAIN_4X);
  apds.setADCGain(apdsColorGain);
  apds.setADCIntegrationTime(apdsIntegrationTimeMs);
  apds.enableProximity(true);
  apds.enableColor(true);
#if defined(APDS_INT_PIN)
  apds.setProximityInterruptThreshold(0, 175);
  apds.enableProximityInterrupt();
  if (!apdsInterruptConfigured)
  {
    pinMode(APDS_INT_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(APDS_INT_PIN), onApdsInterrupt, FALLING);
    apdsInterruptConfigured = true;
  }
#endif
}

bool ensureApdsReady()
{
  if (apdsInitialized && !apdsFaulted)
  {
    return true;
  }

  if (apdsFaulted)
  {
    return false;
  }

  apdsInitialized = apds.begin();
  apdsFaulted = !apdsInitialized;
  if (!apdsInitialized)
  {
    LOG_PROX_LN("APDS lazy init: begin() failed");
    return false;
  }

  configureApdsRuntime();
  lastKnownProximityValid = false;
  LOG_PROX_LN("APDS lazy init: begin() succeeded");
  return true;
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
  lastKnownProximityValid = false;
#if DEBUG_MODE
  Serial.println("Proximity sensor initialized!");
#endif
  LOG_PROX_LN("APDS init: begin() succeeded");
  configureApdsRuntime();
  updateAmbientLightSample();
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

bool readAmbientProximity(unsigned long now, uint8_t &proximityOut, uint32_t *transactionMicros)
{
#if defined(APDS_AVAILABLE)
  if (!ensureApdsReady())
  {
    LOG_PROX_LN("APDS not initialized or faulted; skipping proximity read");
    return false;
  }

#if defined(APDS_INT_PIN)
  const bool interruptPending = apdsProximityInterruptPending;
#else
  const bool interruptPending = false;
#endif

  if (!interruptPending &&
      lastKnownProximityValid &&
      static_cast<unsigned long>(now - lastProximitySampleMs) < LF_APDS_PROX_SAMPLE_INTERVAL_MS)
  {
    proximityOut = lastKnownProximityValue;
    if (transactionMicros != nullptr)
    {
      *transactionMicros = 0;
    }
    return false;
  }

  lastApdsProximityCheck = now;
  const uint32_t startMicros = micros();
  proximityOut = apds.readProximity();
#if defined(APDS_INT_PIN)
  apds.clearInterrupt();
  apdsProximityInterruptPending = false;
#endif
  const uint32_t elapsedMicros = micros() - startMicros;
  lastKnownProximityValue = proximityOut;
  lastKnownProximityValid = true;
  lastProximitySampleMs = now;
  if (transactionMicros != nullptr)
  {
    *transactionMicros = elapsedMicros;
  }
  perfTelemetryRecordDuration(PerfDurationId::ApdsTransaction, elapsedMicros);
  return true;
#else
  (void)proximityOut;
  (void)transactionMicros;
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
  return static_cast<unsigned long>(now - lastAmbientSampleMs) < guardMs;
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
