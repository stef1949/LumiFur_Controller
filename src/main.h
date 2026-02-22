#ifndef MAIN_H
#define MAIN_H

#include <FastLED.h>
#include "userPreferences.h"
#include "deviceConfig.h"
#include "ble/ble.h"
#include "SPI.h"
#include "xtensa/core-macros.h"
#include "bitmaps.h"
#include <stdio.h>
#include "esp_ota_ops.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <esp_task_wdt.h>
#include <cmath>

// #include "customFonts/lequahyper20pt7b.h" // Stylized font
// #include <Fonts/FreeSansBold18pt7b.h>     // Larger font
// #include <Fonts/FreeMonoBold12pt7b.h>    // Smaller font
// #include <Fonts/Picopixel.h>     // Smallest font
#include <Fonts/TomThumb.h> // Smallest font
// #include <Fonts/FreeMonoBold9pt7b.h> // Small font
#include <Fonts/FreeSans9pt7b.h> // Medium font

#define BAUD_RATE 115200 // serial debug port baud rate
// #define CONFIG_BT_NIMBLE_PINNED_TO_CORE 1 // Pinning NimBLE to core 1

// Enum for all available views. This allows for automatic counting.
enum View
{
  VIEW_DEBUG_SQUARES,
  VIEW_LOADING_BAR,
  VIEW_PATTERN_PLASMA,
  VIEW_TRANS_FLAG,
  VIEW_LGBT_FLAG, // NOTE: Commented out as requires re-ordering
  VIEW_NORMAL_FACE,
  VIEW_BLUSH_FACE,
  VIEW_SEMICIRCLE_EYES,
  VIEW_X_EYES,
  VIEW_SLANT_EYES,
  VIEW_SPIRAL_EYES,
  VIEW_PLASMA_FACE,
  VIEW_UWU_EYES,
  VIEW_STARFIELD,
  VIEW_BSOD,
  VIEW_DVD_LOGO,
  VIEW_FLAME_EFFECT,
  VIEW_FLUID_EFFECT,
  VIEW_CIRCLE_EYES,
  VIEW_FULLSCREEN_SPIRAL_PALETTE,
  VIEW_FULLSCREEN_SPIRAL_WHITE,
  VIEW_SCROLLING_TEXT,
  VIEW_DINO_GAME,
  VIEW_PIXEL_DUST,
  VIEW_STATIC_COLOR,
  VIEW_RAINBOW_GRADIENT,
  //VIEW_LGBT_FLAG

  TOTAL_VIEWS // Special entry will automatically hold the total number of views.
};

// Global variables to store the accessory settings.
bool autoBrightnessEnabled = true;
bool accelerometerEnabled = true;
bool sleepModeEnabled = true;
bool auroraModeEnabled = true;
bool staticColorModeEnabled = false;
bool constantColorConfig = false;
CRGB constantColor = CRGB::Green; // Default color for constant color mode
void ensureStaticColorLoaded();
CRGB getStaticColorCached();
// Config variables ------------------------------------------------
bool configApplyAutoBrightness = true; // Variable to track auto brightness application
bool configApplySleepMode = false;
bool configApplyAuroraMode = false;
bool configApplyAccelerometer = true;
bool configApplyConstantColor = false; // Variable to track constant color application

/*------------------------------------------------------------------------------
  OTA instances & variables
  ----------------------------------------------------------------------------*/
static esp_ota_handle_t otaHandler = 0;
static const esp_partition_t *update_partition = NULL;

uint8_t txValue = 0;
int bufferCount = 0;
bool downloadFlag = false;

////////////////////// DEBUG MODE //////////////////////
#define DEBUG_MODE 0          // Set to 1 to enable debug outputs
#define DEBUG_MICROPHONE 0    // Set to 1 to enable microphone debug outputs
#define DEBUG_ACCELEROMETER 0 // Set to 1 to enable accelerometer debug outputs
#define DEBUG_BRIGHTNESS 1    // Set to 1 to enable brightness debug outputs
#define DEBUG_VIEWS 0         // Set to 1 to enable views debug outputs
#define DEBUG_VIEW_TIMING 0   // Set to 1 to enable views debug outputs
#define DEBUG_FPS_COUNTER 0   // Set to 1 to enable FPS counter debug outputs
#define DEBUG_PROXIMITY 0     // Set to 1 to enable proximity sensor debug logs
#define TEXT_DEBUG 1          // Set to 1 to enable text debug outputs
#define DEBUG_FLUID_EFFECT 0  // Set to 1 to enable fluid effect debug outputs
#if DEBUG_MODE
#define DEBUG_BLE
// #define DEBUG_VIEWS
#endif
////////////////////////////////////////////////////////

#if DEBUG_PROXIMITY
#define LOG_PROX(...) Serial.printf(__VA_ARGS__)
#define LOG_PROX_LN(msg) Serial.println(msg)
#else
#define LOG_PROX(...) \
  do                  \
  {                   \
  } while (0)
#define LOG_PROX_LN(msg) \
  do                     \
  {                      \
  } while (0)
#endif

// Button config --------------------------------------------------------------
bool debounceButton(int pin)
{
  static uint32_t lastPressTime = 0;
  uint32_t currentTime = millis();

  if (digitalRead(pin) == LOW && (currentTime - lastPressTime) > 200)
  {
    lastPressTime = currentTime;
    return true;
  }
  return false;
}

// Helper function for ease-in-out quadratic easing
float easeInOutQuad(float t)
{
  return t < 0.5 ? 2 * t * t : -1 + (4 - 2 * t) * t;
}

// Easing functions with bounds checking
float easeInQuad(float t)
{
  if (t < 0.0f)
    return 0.0f;
  if (t > 1.0f)
    return 1.0f;
  return t * t;
}

float easeOutQuad(float t)
{
  if (t < 0.0f)
    return 0.0f;
  if (t > 1.0f)
    return 1.0f;
  return 1.0f - (1.0f - t) * (1.0f - t);
}
// non-blocking LED status functions (Neopixel)
void fadeInAndOutLED(uint8_t r, uint8_t g, uint8_t b)
{
  static int brightness = 0;
  static int step = 5;
  static unsigned long lastUpdate = 0;
  const uint8_t delayTime = 15;

  if (millis() - lastUpdate < delayTime)
    return;
  lastUpdate = millis();

  brightness += step;
  if (brightness >= 128 || brightness <= 0)
    step *= -1;

  statusPixel.setPixelColor(0,
                            (r * brightness) / 255,
                            (g * brightness) / 255,
                            (b * brightness) / 255);
  statusPixel.show();
}

void fadeBlueLED()
{
  fadeInAndOutLED(0, 0, 100); // Blue color
}

// FastLED functions
CRGB currentColor;
CRGBPalette16 palettes[] = {ForestColors_p, LavaColors_p, HeatColors_p, RainbowColors_p, CloudColors_p, PartyColors_p};
CRGBPalette16 currentPalette = palettes[0];

CRGB ColorFromCurrentPalette(uint8_t index = 0, uint8_t brightness = 255, TBlendType blendType = LINEARBLEND)
{
  return ColorFromPalette(currentPalette, index, brightness, blendType);
}

// Helper functions for drawing text and graphics ----------------------------
void buffclear(CRGB *buf);
uint16_t XY16(uint16_t x, uint16_t y);
void mxfill(CRGB *leds);
uint16_t colorWheel(uint8_t pos);
void drawText(int colorWheelOffset);

// Power management scopes
void reduceCPUSpeed()
{
  setCpuFrequencyMhz(80); // Set CPU frequency to lowest setting (80MHz vs 240MHz default)
  Serial.println("CPU frequency reduced to 80MHz for power saving");
}

void restoreNormalCPUSpeed()
{
  setCpuFrequencyMhz(240); // Set CPU frequency back to default (240MHz)
  Serial.println("CPU frequency restored to 240MHz");
}

#include "driver/temp_sensor.h"
void initTempSensor()
{
  temp_sensor_config_t temp_sensor = TSENS_CONFIG_DEFAULT();
  temp_sensor.dac_offset = TSENS_DAC_L2; // TSENS_DAC_L2 is default; L4(-40°C ~ 20°C), L2(-10°C ~ 80°C), L1(20°C ~ 100°C), L0(50°C ~ 125°C)
  temp_sensor_set_config(temp_sensor);
  temp_sensor_start();
}

// Accelerometer object declaration
Adafruit_LIS3DH accel;

// Sundry globals used for animation ---------------------------------------

// Scrolling text sundries
int16_t textX;                         // Current text position (X)
int16_t textY;                         // Current text position (Y)
int16_t textMin;                       // Text pos. (X) when scrolled off left edge
extern char txt[64];                   // Buffer to hold scrolling message text
extern void initializeScrollingText(); // Exposes function

int16_t ball[3][4] = {
    {3, 0, 1, 1}, // Initial X,Y pos+velocity of 3 bouncy balls
    {17, 15, 1, -1},
    {27, 4, -1, 1}};
uint16_t ballcolor[3]; // Colors for bouncy balls (init in setup())

// Error handler function. If something goes wrong, this is what runs.
void err(int x)
{
  uint8_t i;
  pinMode(LED_BUILTIN, OUTPUT); // Using onboard LED
  for (int i = 1;; i++)
  {                                   // Loop forever...
    digitalWrite(LED_BUILTIN, i & 1); // LED on/off blink to alert user
    delay(x);
  }
}

uint8_t userBrightness = getUserBrightness(); // e.g., default 255 (100%)

int sliderBrightness = map(userBrightness, 1, 255, 1, 100);

// Convert the userBrightness into a scale factor (0.0 to 1.0)
// Here, dividing userBrightness by 255.0 to get a proportion.
extern float globalBrightnessScale;
extern uint16_t globalBrightnessScaleFixed;
void updateGlobalBrightnessScale(uint8_t brightness);

unsigned long lastAmbientUpdateTime = 0;
const unsigned long ambientUpdateInterval = 500; // update every 500 milliseconds
uint8_t adaptiveBrightness = userBrightness;     // current brightness used by the system
uint8_t targetBrightness = userBrightness;       // target brightness for adaptive adjustment

// APDS9960 runtime state
bool apdsInitialized = false;
bool apdsFaulted = false;
volatile bool apdsProximityInterruptFlag = false;
unsigned long lastApdsProximityCheck = 0;
unsigned long lastApdsInitRetry = 0;
constexpr unsigned long APDS_PROX_FALLBACK_MS = 750; // Slow fallback poll when no interrupt
constexpr unsigned long APDS_PROX_CACHE_MS = 50;     // Minimum spacing between proximity fetches
constexpr unsigned long APDS_INIT_RETRY_MS = 2000;   // Retry sensor init if it temporarily faults

// float ambientLux = 0.0;
static uint16_t lastKnownClearValue = 0; // Initialize to a sensible default
static uint16_t lastKnownRedValue = 0;
static uint16_t lastKnownGreenValue = 0;
static uint16_t lastKnownBlueValue = 0;
static float lastKnownLuxValue = 0.0f;
static apds9960AGain_t apdsColorGain = APDS9960_AGAIN_4X;
static uint16_t apdsIntegrationTimeMs = 10;
constexpr uint16_t APDS_LUX_INTEGRATION_MIN_MS = 3;
constexpr uint16_t APDS_LUX_INTEGRATION_MAX_MS = 100;
constexpr uint16_t APDS_CLEAR_LOW_COUNT = 200;
constexpr uint16_t APDS_CLEAR_HIGH_COUNT = 8000;
constexpr float APDS_LUX_REFERENCE_INTEGRATION_MS = 10.0f;
constexpr float APDS_LUX_REFERENCE_GAIN = 4.0f;
float smoothedLux = 50.0f;
const float luxSmoothingFactor = 0.35f;        // Adjust between 0.05 - 0.3
float currentBrightness = 128.0f;              // NEW: Current brightness as a float for smoothing
const float brightnessSmoothingFactor = 0.95f; // NEW: How quickly brightness adapts (0.01-0.1)
const float manualBrightnessSmoothingFactor = 0.04f; // Smooth target changes from manual brightness commands
int lastBrightness = 0;
const int brightnessThreshold = 1; // Only update if change > X
unsigned long lastLuxUpdate = 0;
const unsigned long luxUpdateInterval = 250; // Reduce read frequency to ease I2C contention

#if defined(APDS_AVAILABLE)
void IRAM_ATTR onApdsInterrupt()
{
  apdsProximityInterruptFlag = true;
}
#endif

void setupAdaptiveBrightness()
{
#if defined(APDS_AVAILABLE)
  // Ensure I2C is up on the STEMMA QT pins (MatrixPortal S3: SDA=20, SCL=19)
  Wire.begin(APDS_SDA_PIN, APDS_SCL_PIN);
  Wire.setClock(400000); // Fast-mode for APDS9960
  LOG_PROX_LN("APDS init: starting Wire and begin()");

  if (!apds.begin())
  {
    Serial.println("failed to initialize proximity sensor! Please check your wiring.");
    LOG_PROX_LN("APDS init: begin() failed");
    apdsFaulted = true;
    apdsInitialized = false;
    return;
  }

  apdsInitialized = true;
  apdsFaulted = false;
  Serial.println("Proximity sensor initialized!");
  LOG_PROX_LN("APDS init: begin() succeeded");
  // Boost sensitivity for proximity and color for auto-brightness
  apds.setProxGain(APDS9960_PGAIN_8X);
  // apds.setLEDDrive(APDS9960_LEDDRIVE_100MA);
  apds.setADCGain(apdsColorGain);
  apds.setADCIntegrationTime(apdsIntegrationTimeMs); // Tuning for lux range
  apds.enableProximity(true);                  // enable proximity mode
  apds.enableColor(true);                      // enable color mode
  apds.setProximityInterruptThreshold(0, 175); // set the interrupt threshold to fire when proximity reading goes above 175
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

// Setup functions for adaptive brightness & proximity sensing using APDS9960:
static float apdsGainToFloat(apds9960AGain_t gain)
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

static apds9960AGain_t apdsStepGain(apds9960AGain_t gain, bool increase)
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

static float calculateLuxFromRgb(uint16_t r, uint16_t g, uint16_t b)
{
  float illuminance = (-0.32466f * r) + (1.57837f * g) + (-0.73191f * b);
  if (illuminance < 0.0f)
  {
    illuminance = 0.0f;
  }
  return illuminance;
}

static float scaleLuxForSettings(float luxRaw)
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

static void adjustApdsColorRange(uint16_t clear)
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

static void updateAmbientLightSample()
{
  static unsigned long lastLogTime = 0;
  static unsigned long lastForcedReadTime = 0;
  const unsigned long logInterval = 500; // Log 2 times per second

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

void updateAdaptiveBrightness()
{
  // If auto brightness is disabled, just apply the manual user brightness and exit.
  if (!autoBrightnessEnabled)
  {
    // Manual mode still eases toward the requested brightness instead of stepping instantly.
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
    // Sensor unavailable this cycle: use manual brightness and retry init periodically.
    const unsigned long now = millis();
    if ((now - lastApdsInitRetry) >= APDS_INIT_RETRY_MS)
    {
      lastApdsInitRetry = now;
      apdsFaulted = false; // Allow re-attempt.
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

  // Sample ambient light only at the configured lux interval.
  if (!hasCachedLux || (now - lastLuxUpdate) >= luxUpdateInterval)
  {
    cachedLux = getAmbientLux();
    smoothedLux = (luxSmoothingFactor * cachedLux) + ((1.0f - luxSmoothingFactor) * smoothedLux);
    lastLuxUpdate = now;
    hasCachedLux = true;
  }

  const float currentLux = cachedLux;

  // --- CRITICAL CALIBRATION SECTION ---

  const int min_brightness_output = 80;   // Minimum display brightness
  const int max_brightness_output = 255;  // Preserve full-range capability
  const float min_lux_for_map = 5.0f;     // Dim indoor light
  const float max_lux_for_map = 10000.0f; // Bright outdoor shade

  // ADJUST THESE VALUES:
  // For earlier brightening (brighter in lower lux):
  // const float min_lux_for_map = 20.0f;             // Higher value = starts brightening earlier

  // For later brightening (only brightens in higher lux):
  // const float min_lux_for_map = 1.0f;              // Lower value = only brightens when very dark

  // For reaching full brightness sooner (in less bright conditions):
  // const float max_lux_for_map = 2000.0f;            // Lower value = reaches max brightness sooner

  // For reaching full brightness later (only in very bright conditions):
  // const float max_lux_for_map = 20000.0f;           // Higher value = needs more light for full brightness

  // For different minimum brightness level:
  // const int min_brightness_output = 30;             // Fixed minimum instead of user brightness

  // For different maximum brightness level:
  // const int max_brightness_output = 200;            // Cap maximum brightness below 255

  // --- END CRITICAL CALIBRATION SECTION ---

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
    uint8_t brightnessToApply = static_cast<uint8_t>(smoothedBrightness);
    dma_display->setBrightness8(brightnessToApply);
    updateGlobalBrightnessScale(brightnessToApply);
    lastBrightness = smoothedBrightness; // Update lastBrightness ONLY when a display change is made
#if DEBUG_BRIGHTNESS
    Serial.printf(">>>> ADAPT: BRIGHTNESS SET TO %d <<<<\n", smoothedBrightness);
#endif
  }
  else
  {
    // Change is below threshold: keep the previously applied brightness (no-op)
    // No action needed; brightness remains unchanged.
    // lastBrightness remains unchanged because we didn't apply a new value
#if DEBUG_BRIGHTNESS
    Serial.printf(">>>> ADAPT: BRIGHTNESS KEPT AT %d <<<<\n", lastBrightness);
#endif
  }
}

void maybeUpdateBrightness()
{
  // Always update brightness smoothing; updateAdaptiveBrightness handles lux sampling cadence internally.
  updateAdaptiveBrightness();
}

inline bool hasElapsedSince(unsigned long now, unsigned long last, unsigned long interval)
{
  return static_cast<unsigned long>(now - last) >= interval;
}

inline void markApdsFault()
{
  apdsFaulted = true;
  apdsInitialized = false;
}

bool shouldReadProximity(unsigned long now)
{
#if defined(APDS_AVAILABLE)
  // If the sensor never came up at boot, try once on demand.
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
      markApdsFault();
    }
  }

  if (!apdsInitialized || apdsFaulted)
  {
    LOG_PROX_LN("APDS not initialized or faulted; skipping proximity read");
    return false;
  }

  if (now - lastApdsProximityCheck < APDS_PROX_CACHE_MS)
  {
    return false; // Avoid back-to-back reads
  }

  lastApdsProximityCheck = now;

  return true;
#else
  (void)now;
  return false;
#endif
}

constexpr std::size_t color_num = 5;
using colour_arr_t = std::array<uint16_t, color_num>;

uint16_t myDARK, myWHITE, myRED, myGREEN, myBLUE;
colour_arr_t colours;

struct Square
{
  float xpos, ypos;
  float velocityx;
  float velocityy;
  boolean xdir, ydir;
  uint16_t square_size;
  uint16_t colour;
};

const int numSquares = 25;
Square Squares[numSquares];

// Add the new helper function after the ConfigCallbacks class definition:
void applyConfigOptions()
{
  Serial.println("Applying configuration options...");

  if (autoBrightnessEnabled)
  {
    Serial.println("Auto Brightness enabled: adjusting brightness automatically.");
    // Example: call a function to update auto brightness (if implemented)
    // updateAutoBrightness();
    configApplyAutoBrightness = true;
  }
  else
  {
    Serial.println("Auto brightness disabled. Applying user-set brightness.");
    dma_display->setBrightness8(userBrightness);
    updateGlobalBrightnessScale(userBrightness);
    lastBrightness = userBrightness;
    currentBrightness = static_cast<float>(userBrightness);
    Serial.printf("Applied manual brightness: %u\n", userBrightness);
  }

  if (accelerometerEnabled)
  {
    Serial.println("Accelerometer enabled.");
    // Insert code to enable accelerometer-driven actions here.
    configApplyAccelerometer = true;
  }
  else
  {
    Serial.println("Accelerometer disabled.");
    // Optionally disable or ignore accelerometer actions.
    configApplyAccelerometer = false;
  }

  if (sleepModeEnabled)
  {
    Serial.println("Sleep mode enabled: device can enter sleep mode.");
    // Update sleep-related thresholds or enable sleep mode triggers.
    configApplySleepMode = true;
  }
  else
  {
    Serial.println("Sleep mode disabled: forcing device to remain awake.");
    configApplySleepMode = false; // Ensure the device remains awake when sleep mode is disabled.
  }

  if (staticColorModeEnabled)
  {
    Serial.println("Static color mode enabled: using user-selected color.");
    constantColor = getStaticColorCached();
    constantColorConfig = true;
    configApplyConstantColor = true;
    configApplyAuroraMode = false;
  }
  else
  {
    constantColorConfig = false;
    configApplyConstantColor = false;
    if (auroraModeEnabled)
    {
      Serial.println("Aurora mode enabled: switching to aurora palette.");
      // Assume auroraPalette and defaultPalette are defined globally.
      // currentPalette = auroraPalette;
      configApplyAuroraMode = true;
    }
    else
    {
      Serial.println("Aurora mode disabled: using default palette.");
      // currentPalette = defaultPalette;
      configApplyAuroraMode = false;
    }
  }
}

// Startfield Animation -----------------------------------------------
struct Star
{
  int x;
  int y;
  int speed;
};
const int NUM_STARS = 50;
Star stars[NUM_STARS];

void initStarfieldAnimation()
{
  for (int i = 0; i < NUM_STARS; i++)
  {
    stars[i].x = random(0, dma_display->width());
    stars[i].y = random(0, dma_display->height());
    stars[i].speed = random(1, 4); // Stars move at different speeds
  }
}

struct DVDLogo
{
  int x, y;
  int vx, vy;
  uint16_t color;
};

DVDLogo logos[2];

const int dvdWidth = 23; // Width of the DVD logo
const int dvdHeight = 10;

unsigned long lastDvdUpdate = 0;
const unsigned long dvdUpdateInterval = 20;

// First logo (left panel)
int dvdX1 = 0, dvdY1 = 0;
int dvdVX1 = 1, dvdVY1 = 1;
uint16_t dvdColor1;

// Second logo (right panel)
int dvdX2 = 64, dvdY2 = 0;
int dvdVX2 = 1, dvdVY2 = 1;
uint16_t dvdColor2;

#endif /* MAIN_H */
