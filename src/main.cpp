#if defined(ESP_PLATFORM) && !defined(ARDUINO)
// Only include ESP-IDF headers when building as a pure ESP-IDF app, not under Arduino-ESP32.
#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/gpio.h>
#include "sdkconfig.h"
#include "esp_assert.h"
#endif

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Wire.h> // For I2C sensors
#ifdef VIRTUAL_PANE
#include <ESP32-VirtualMatrixPanel-I2S-DMA.h>
#else
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#endif

#include <Adafruit_PixelDust.h> // For sand simulation
#include "main.h"
#include "dino_game.h"
#include "core/mic/mic.h"
#include "core/ColorParser.h"
#include "customEffects/flameEffect.h"
#include "customEffects/fluidEffect.h"
#include "core/AnimationState.h"
#include "core/ScrollState.h"
// #include "customEffects/pixelDustEffect.h" // New effect

// BLE Libraries
#include <NimBLEDevice.h>

#include <vector> //For chunking example
#include <cstring>
#include <string>
#include <cctype>
#include <cstdio>
#include <cstdlib>

// #define PIXEL_COLOR_DEPTH_BITS 16 // 16 bits per pixel
#if DEBUG_MODE
#define LOG_DEBUG(...) Serial.printf(__VA_ARGS__)
#define LOG_DEBUG_LN(message) Serial.println(message)
#else
#define LOG_DEBUG(...) \
  do                   \
  {                    \
  } while (0)
#define LOG_DEBUG_LN(message) \
  do                          \
  {                           \
  } while (0)
#endif

// #include "EffectsLayer.hpp" // FastLED CRGB Pixel Buffer for which the patterns are drawn
// EffectsLayer effects(VPANEL_W, VPANEL_H);

// ----------------------------------------------------------------
// ----------------------------------------------------------------
// ------------------- LumiFur Global Variables -------------------
// ----------------------------------------------------------------
// ----------------------------------------------------------------
FluidEffect *fluidEffectInstance = nullptr; // Global pointer for our fluid effect object
// FluidEffect* fluidEffectInstance = nullptr; // Keep or remove if replacing
// PixelDustEffect* pixelDustEffectInstance = nullptr; // Add this
// Brightness control
//  Retrieve the brightness value from preferences
bool g_accelerometer_initialized = false;

constexpr unsigned long MOTION_SAMPLE_INTERVAL_FAST = 15;  // ms between accel reads when looking for shakes

// --- Performance Tuning ---
// Target ~50-60 FPS. Adjust as needed based on view complexity.
const unsigned long targetFrameIntervalMillis = 12;            // ~100 FPS pacing
constexpr uint32_t SLOW_FRAME_THRESHOLD_US = targetFrameIntervalMillis * 1000UL;

#if DEBUG_MODE
constexpr uint32_t SLOW_SECTION_THRESHOLD_US = 2000; // Flag non-render work that takes >2ms
#define CONCAT_INNER(a, b) a##b
#define CONCAT(a, b) CONCAT_INNER(a, b)
struct ScopedSectionTimer
{
  const char *label;
  const uint32_t startMicros;
  ScopedSectionTimer(const char *name) : label(name), startMicros(micros()) {}
  ~ScopedSectionTimer()
  {
    const uint32_t elapsed = micros() - startMicros;
    if (elapsed > SLOW_SECTION_THRESHOLD_US)
    {
      Serial.printf("Slow section %s: %lu us\n", label, static_cast<unsigned long>(elapsed));
    }
  }
};
#define PROFILE_SECTION(name) ScopedSectionTimer CONCAT(sectionTimer_, __LINE__)(name)
#else
#define PROFILE_SECTION(name) \
  do                          \
  {                           \
  } while (0)
#endif
#if DEBUG_MODE
volatile uint32_t gLastFrameDurationMicros = 0; // Last full frame render time
volatile uint32_t gWorstFrameDurationMicros = 0;
#endif
// ---

bool brightnessChanged = false;

constexpr unsigned long AUTO_BRIGHTNESS_INTERVAL_MS = 250;
constexpr unsigned long LUX_UPDATE_INTERVAL_MS = 500;
constexpr unsigned long SLEEP_CHECK_INTERVAL_MS = 60;
constexpr unsigned long PAIRING_RESET_HOLD_MS = 3000;
constexpr unsigned long PAIRING_RESET_DEBOUNCE_MS = 50;

static unsigned long lastAutoBrightnessUpdate = 0;
static unsigned long lastLuxUpdateTime = 0;
static unsigned long lastSleepCheckTime = 0;

// View switching
volatile uint8_t currentView = VIEW_FLAME_EFFECT; // Current view (volatile so the display task sees updates)
const int totalViews = TOTAL_VIEWS;      // Total number of views is now calculated automatically
// int userBrightness = 20; // Default brightness level (0-255)

// Plasma animation optimization
uint16_t plasmaFrameDelay = 15; // ms between plasma updates (was 10)
// Spiral animation optimization
unsigned long rotationFrameInterval = 15; // ms between spiral rotation updates (was 11)

using animation::AnimationState;
using animation::BlinkTimings;
using animation::BlushState;

AnimationState &gAnimationState = animation::state();

BlinkTimings &blinkState = gAnimationState.blinkTimings;
unsigned long &lastEyeBlinkTime = gAnimationState.lastEyeBlinkTime;
unsigned long &nextBlinkDelay = gAnimationState.nextBlinkDelay;
int &blinkProgress = gAnimationState.blinkProgress;
bool &isBlinking = gAnimationState.isBlinking;
bool &manualBlinkTrigger = gAnimationState.manualBlinkTrigger;
bool &isEyeBouncing = gAnimationState.isEyeBouncing;
unsigned long &eyeBounceStartTime = gAnimationState.eyeBounceStartTime;
volatile int &currentEyeYOffset = gAnimationState.currentEyeYOffset;
int &eyeBounceCount = gAnimationState.eyeBounceCount;
uint8_t &viewBeforeEyeBounce = gAnimationState.viewBeforeEyeBounce;
volatile int &idleEyeYOffset = gAnimationState.idleEyeYOffset;
volatile int &idleEyeXOffset = gAnimationState.idleEyeXOffset;
unsigned long &spiralStartMillis = gAnimationState.spiralStartMillis;
int &previousView = gAnimationState.previousView;
int &currentMaw = gAnimationState.currentMaw;
unsigned long &mawChangeTime = gAnimationState.mawChangeTime;
bool &mawTemporaryChange = gAnimationState.mawTemporaryChange;
bool &mouthOpen = gAnimationState.mouthOpen;
unsigned long &lastMouthTriggerTime = gAnimationState.lastMouthTriggerTime;
volatile BlushState &blushState = gAnimationState.blushState;
volatile unsigned long &blushStateStartTime = gAnimationState.blushStateStartTime;
bool &isBlushFadingIn = gAnimationState.isBlushFadingIn;
uint8_t &originalViewBeforeBlush = gAnimationState.originalViewBeforeBlush;
bool &wasBlushOverlay = gAnimationState.wasBlushOverlay;
volatile uint8_t &blushBrightness = gAnimationState.blushBrightness;

float globalBrightnessScale = 0.0f;
uint16_t globalBrightnessScaleFixed = 256;
bool facePlasmaDirty = true;

inline void updateGlobalBrightnessScale(uint8_t brightness)
{
  globalBrightnessScale = brightness / 255.0f;
  globalBrightnessScaleFixed = static_cast<uint16_t>((static_cast<uint32_t>(brightness) * 256u + 127u) / 255u);
  facePlasmaDirty = true;
}

struct StaticColorState
{
  CRGB color = CRGB::Black;
  bool hasValue = false;
};

static StaticColorState gStaticColorState;

static CRGB toCRGB(const RgbColor &color)
{
  return CRGB(color.r, color.g, color.b);
}

static RgbColor toRgbColor(const CRGB &color)
{
  RgbColor result;
  result.r = color.r;
  result.g = color.g;
  result.b = color.b;
  return result;
}

static bool parseColorPayloadToCRGB(const uint8_t *data, size_t length, CRGB &colorOut, std::string &normalizedHex)
{
  RgbColor parsed;
  if (!parseColorPayload(data, length, parsed, normalizedHex))
  {
    return false;
  }
  colorOut = toCRGB(parsed);
  return true;
}

static void setStaticColor(const CRGB &color)
{
  gStaticColorState.color = color;
  gStaticColorState.hasValue = true;
  constantColor = color;
}

  static void setStaticColorToDefault()
  {
    setStaticColor(constantColor);
  }

  static bool applyStaticColorBytes(const uint8_t *data, size_t length, bool persistPreference, std::string *normalizedOut)
  {
    std::string normalized;
    CRGB parsedColor;
    if (!parseColorPayloadToCRGB(data, length, parsedColor, normalized))
    {
      return false;
    }

    setStaticColor(parsedColor);
    if (persistPreference)
    {
      saveSelectedColor(String(normalized.c_str()));
    }
    if (normalizedOut)
    {
      *normalizedOut = normalized;
    }
    return true;
  }

static bool loadStaticColorFromPreferences()
{
  String stored = getSelectedColor();
  if (stored.length() == 0)
  {
    setStaticColorToDefault();
    return false;
  }

  std::string rawValue;
  rawValue.reserve(stored.length());
  for (unsigned int i = 0; i < stored.length(); ++i)
  {
    rawValue.push_back(static_cast<char>(stored[i]));
  }

  if (!applyStaticColorBytes(reinterpret_cast<const uint8_t *>(rawValue.data()), rawValue.size(), false, nullptr))
  {
    setStaticColorToDefault();
    return false;
  }
  return true;
}

void ensureStaticColorLoaded()
{
  if (!gStaticColorState.hasValue)
  {
    loadStaticColorFromPreferences();
  }
}

  CRGB getStaticColorCached()
  {
    ensureStaticColorLoaded();
    return gStaticColorState.color;
  }

  static String getStaticColorHexString()
  {
    ensureStaticColorLoaded();
    const std::string hex = colorToHexString(toRgbColor(gStaticColorState.color));
    return String(hex.c_str());
  }

inline void setBlinkProgress(int newValue)
{
  if (blinkProgress != newValue)
  {
    facePlasmaDirty = true;
  }
  blinkProgress = newValue;
}

constexpr int MIN_BLINK_DURATION = 300;
constexpr int MAX_BLINK_DURATION = 800;
constexpr int MIN_BLINK_DELAY = 250;
constexpr int MAX_BLINK_DELAY = 5000;

constexpr unsigned long EYE_BOUNCE_DURATION = 400;
constexpr int EYE_BOUNCE_AMPLITUDE = 5;
constexpr int MAX_EYE_BOUNCES = 10;

const int IDLE_HOVER_AMPLITUDE_Y = 1;
const int IDLE_HOVER_AMPLITUDE_X = 1;
const unsigned long IDLE_HOVER_PERIOD_MS_Y = 3000;
const unsigned long IDLE_HOVER_PERIOD_MS_X = 4200;

const int totalMaws = 2;
const unsigned long mouthOpenHoldTime = 300;

const int minBlinkDuration = MIN_BLINK_DURATION;
const int maxBlinkDuration = MAX_BLINK_DURATION;
const int minBlinkDelay = MIN_BLINK_DELAY;
const int maxBlinkDelay = MAX_BLINK_DELAY;

int loadingProgress = 0;
int loadingMax = 100;


template <typename Callback>
inline void runIfElapsed(unsigned long now, unsigned long &last, unsigned long interval, Callback &&callback)
{
  if (hasElapsedSince(now, last, interval))
  {
    last = now;
    callback();
  }
}

char txt[64];
constexpr uint16_t SCROLL_BACKGROUND_INTERVAL_MS = 90;
constexpr int16_t SCROLL_TEXT_GAP = 12;

// Global constants for each sensitivity level
const float SLEEP_THRESHOLD = 1.0; // for sleep mode detection
const float SHAKE_THRESHOLD = 1.0; // for shake detection
// Global flag to control which threshold to use
bool useShakeSensitivity = true;

// Blush effect variables
const unsigned long fadeInDuration = 2000; // Duration for fade-in (2 seconds)
const unsigned long fullDuration = 6000;   // Full brightness time after fade-in (6 seconds)
// Total time from trigger to start fade-out is fadeInDuration + fullDuration = 8 seconds.
const unsigned long fadeOutDuration = 2000; // Duration for fade-out (2 seconds)

// Non-blocking sensor read interval
unsigned long lastSensorReadTime = 0;
const unsigned long sensorInterval = 250; // sensor read throttled to reduce I2C pressure
constexpr uint8_t PROX_TRIGGER_DELTA = 6;     // How far above baseline to trigger
constexpr uint8_t PROX_RELEASE_DELTA = 2;     // How close to baseline to release
constexpr unsigned long PROX_LATCH_TIMEOUT_MS = 1200;
constexpr float PROX_BASELINE_ALPHA = 0.05f;  // Baseline smoothing factor
static bool proximityLatchedHigh = false;     // Prevents retriggering until sensor settles
static unsigned long proximityLatchedAt = 0;
static float proximityBaseline = 0.0f;
static bool proximityBaselineValid = false;

// Variables for plasma effect
uint16_t time_counter = 0, cycles = 0, fps = 0;
unsigned long fps_timer;

// Sleep mode variables
// Define both timeout values and select the appropriate one in setup()
const unsigned long SLEEP_TIMEOUT_MS_DEBUG = 600000;  // 15 seconds (15,000 ms)
const unsigned long SLEEP_TIMEOUT_MS_NORMAL = 600000; // 10 minutes (300,000 ms)
unsigned long SLEEP_TIMEOUT_MS;                       // Will be set in setup() based on debugMode
bool sleepModeActive = false;
unsigned long lastActivityTime = 0;
float prevAccelX = 0, prevAccelY = 0, prevAccelZ = 0;
static float lastAccelDeltaX = 0, lastAccelDeltaY = 0, lastAccelDeltaZ = 0;
static bool accelSampleValid = false;
static bool lastMotionAboveShake = false;
static bool lastMotionAboveSleep = false;
uint8_t preSleepView = VIEW_NORMAL_FACE;                  // Store the view before sleep
uint8_t sleepBrightness = 15;              // Brightness level during sleep (0-255)
uint8_t normalBrightness = userBrightness; // Normal brightness level
volatile bool notifyPending = false;
unsigned long sleepFrameInterval = 50; // Frame interval in ms (will be changed during sleep)
unsigned long bluetoothStatusChangeMillis = 0;

// FPS Calculation (moved out of drawing function)
volatile int currentFPS = 0; // Use volatile if accessed by ISR, though not needed here
unsigned long lastFpsCalcTime = 0;
int frameCounter = 0;

// Full Screen Spinning Spiral Effect Variables (NEW)
float fullScreenSpiralAngle = 0.0f;
uint8_t fullScreenSpiralColorOffset = 0;
unsigned long lastFullScreenSpiralUpdateTime = 0;
// const unsigned long FULL_SCREEN_SPIRAL_FRAME_INTERVAL_MS = 50; // Adjusted for performance
const unsigned long FULL_SCREEN_SPIRAL_FRAME_INTERVAL_MS = 0; // Further adjusted if needed

const float FULL_SPIRAL_ROTATION_SPEED_RAD_PER_UPDATE = 0.2f;
const uint8_t FULL_SPIRAL_COLOR_SCROLL_SPEED = 2.5;

// Logarithmic Spiral Coefficients (NEW/MODIFIED)
const float LOG_SPIRAL_A_COEFF = 4.0f;  // Initial radius factor (when theta is small or exp(0))
                                        // Smaller 'a' means it starts smaller/tighter from center.
const float LOG_SPIRAL_B_COEFF = 0.10f; // Controls how tightly wound the spiral is.
                                        // Smaller 'b' makes it wind more slowly (appearing tighter initially for a given theta range).
                                        // Larger 'b' makes it expand much faster.
                                        // Positive 'b' for outward spiral, negative for inward.

const float SPIRAL_ARM_COLOR_FACTOR = 5.0f; // Could be based on 'r' now instead of 'theta_arm'
const float SPIRAL_THICKNESS_RADIUS = 1.0f; // Start with 0 for performance, then increase

static TaskHandle_t bleNotifyTaskHandle = NULL;

inline void notifyBleTask()
{
  if (deviceConnected && bleNotifyTaskHandle)
  {
    xTaskNotifyGive(bleNotifyTaskHandle);
  }
}

struct _InitScrollSpeed
{
  _InitScrollSpeed() { scroll::updateIntervalFromSpeed(); }
} _initScrollSpeedOnce; // Ensure scroll interval is set at startup

void calculateFPS()
{
  frameCounter++;
  unsigned long now = millis();
  if (now - lastFpsCalcTime >= 1000)
  {
    currentFPS = frameCounter;
    frameCounter = 0;
    lastFpsCalcTime = now;
    // Serial.printf("FPS: %d\n", currentFPS); // Optional: Print FPS once per second
  }
}

void maybeUpdateTemperature()
{
  if (!deviceConnected)
  {
    return;
  }

  static unsigned long lastTempUpdate = 0;
  const unsigned long tempUpdateInterval = 5000;       // 5 seconds
  const unsigned long sleepTempUpdateInterval = 10000; // 10 seconds
  const unsigned long now = millis();
  const unsigned long targetInterval = sleepModeActive ? sleepTempUpdateInterval : tempUpdateInterval;

  if (now - lastTempUpdate >= targetInterval)
  {
    updateTemperature();
    lastTempUpdate = now;
  }
}

void displayCurrentView(int view);
static bool viewUsesMic(int view)
{
  switch (view)
  {
  case VIEW_NORMAL_FACE:
  case VIEW_BLUSH_FACE:
  case VIEW_SEMICIRCLE_EYES:
  case VIEW_X_EYES:
  case VIEW_SLANT_EYES:
  case VIEW_SPIRAL_EYES:
  case VIEW_PLASMA_FACE:
  case VIEW_UWU_EYES:
  case VIEW_CIRCLE_EYES:
    return true;
  default:
    return false;
  }
}

void wakeFromSleepMode()
{
  if (!sleepModeActive)
    return; // Already awake

  Serial.println(">>> Waking from sleep mode <<<");
  sleepModeActive = false;
  currentView = preSleepView; // Restore previous view
  micSetEnabled(viewUsesMic(currentView));

  // Restore normal CPU speed
  restoreNormalCPUSpeed();

  // Restore normal frame rate
  sleepFrameInterval = 5; // Back to ~90 FPS

  // Restore normal brightness
  dma_display->setBrightness8(userBrightness);
  updateGlobalBrightnessScale(userBrightness);
  lastBrightness = userBrightness;
  currentBrightness = static_cast<float>(userBrightness);

  lastActivityTime = millis(); // Reset activity timer

  // Notify all clients if connected
  if (deviceConnected)
  {
    // Also send current view back to the app
    uint8_t bleViewValue = static_cast<uint8_t>(currentView);
    pFaceCharacteristic->setValue(&bleViewValue, 1);
    pFaceCharacteristic->notify();

    // Send a temperature update
    maybeUpdateTemperature();
  }

  // Restore normal BLE advertising if not connected
  if (!deviceConnected)
  {
    NimBLEDevice::getAdvertising()->setMinInterval(160); // 100 ms (default)
    NimBLEDevice::getAdvertising()->setMaxInterval(240); // 150 ms (default)
    NimBLEDevice::startAdvertising();
  }
}

// Callback class for handling writes to the command characteristic
class CommandCallbacks : public NimBLECharacteristicCallbacks
{

  // Override the onWrite method
  void onWrite(NimBLECharacteristic *pCharacteristic)
  {
    // Get the value written by the client
    std::string rxValue = pCharacteristic->getValue();

    if (rxValue.length() > 0)
    {
      uint8_t commandCode = rxValue[0]; // Assuming single-byte commands

      Serial.print("Command Characteristic received write, command code: 0x");
      Serial.println(commandCode, HEX);

      // Process the command
      switch (commandCode)
      {
      case 0x01: // Command: Get Temperature History
        Serial.println("-> Command: Get Temperature History");
        triggerHistoryTransfer(); // Call the function to start sending history
        break;

      case 0x02: // Example: Command: Clear History Buffer
        Serial.println("-> Command: Clear History Buffer");
        clearHistoryBuffer();
        break;

        // Add more cases for other commands here...

      default:
        Serial.print("-> Unknown command code received: 0x");
        Serial.println(commandCode, HEX);
        break;
      }
    }
    else
    {
      Serial.println("Command Characteristic received empty write.");
    }
  }
} cmdCallbacks;

// New Callback class for handling notifications related to Temperature Logs
class TemperatureLogsCallbacks : public NimBLECharacteristicCallbacks
{
public:
  // Optionally, implement onSubscribe to log when a client subscribes to notifications
  void onSubscribe(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &connInfo, uint16_t subValue) override
  {
    std::string str = "Client subscribed to Temperature Logs notifications, Client ID: ";
    str += connInfo.getConnHandle();
    str += " Address: ";
    str += connInfo.getAddress().toString();
    Serial.printf("%s\n", str.c_str());
  }
};
TemperatureLogsCallbacks logsCallbacks;

// Class to handle characteristic callbacks
class CharacteristicCallbacks : public NimBLECharacteristicCallbacks
{
  void onRead(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &connInfo) override
  {
    uint8_t bleViewValue = static_cast<uint8_t>(currentView);
    pCharacteristic->setValue(&bleViewValue, 1);
    Serial.printf("Read request - returned view: %d\n", bleViewValue);
  }

  void onWrite(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &connInfo)
  {
    std::string value = pCharacteristic->getValue();
    if (value.length() < 1)
      return;

    uint8_t newView = value[0];

    // --- Handle Wake-up First ---
    if (sleepModeActive)
    {
      Serial.println("BLE write received while sleeping, waking up..."); // DEBUG
      wakeFromSleepMode();
      // NOTE: wakeFromSleepMode resets lastActivityTime and sets sleepModeActive = false
      // Now that we are awake, proceed to process the command below.
    }
    // Normal view change
    if (newView >= 0 && newView < totalViews && newView != currentView)
    {
      currentView = newView;
      saveLastView(currentView);   // Persist the new view
      lastActivityTime = millis(); // BLE command counts as activity
      Serial.printf("Write request - new view: %d\n", currentView);
      // pCharacteristic->notify();
      // notifyPending = true;
      notifyBleTask();
    }
    else if (newView >= totalViews)
    {
      Serial.printf("BLE Write ignored - invalid view number: %d (Total Views: %d)\n", newView, totalViews); // DEBUG
    }
    // If newView == currentView, we still woke up (if sleeping), but don't change the view or notify
  }

  void onStatus(NimBLECharacteristic *pCharacteristic, int code) override
  {
    Serial.printf("Notification/Indication return code: %d, %s\n", code, NimBLEUtils::returnCodeToString(code));
  }

  void onSubscribe(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &connInfo, uint16_t subValue) override
  {
    std::string str = "Client ID: ";
    str += connInfo.getConnHandle();
    str += " Address: ";
    str += connInfo.getAddress().toString();
    if (subValue == 0)
    {
      str += " Unsubscribed to ";
    }
    else if (subValue == 1)
    {
      str += " Subscribed to notifications for ";
    }
    else if (subValue == 2)
    {
      str += " Subscribed to indications for ";
    }
    else if (subValue == 3)
    {
      str += " Subscribed to notifications and indications for ";
    }
    str += std::string(pCharacteristic->getUUID());

    Serial.printf("%s\n", str.c_str());
  }
} chrCallbacks;

// Custom characteristic callback class.
class ConfigCallbacks : public NimBLECharacteristicCallbacks
{
  void onWrite(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &connInfo) override
  {
    std::string value = pCharacteristic->getValue();
    Serial.println("Received configuration update:");
    // Expect at least 4 bytes (legacy) or 5 bytes (adds static color mode).
    if (value.length() >= 4)
    {
      bool oldAutoBrightnessEnabled = autoBrightnessEnabled; // Store old state
      bool oldAuroraMode = auroraModeEnabled;
      bool oldStaticColorMode = staticColorModeEnabled;
      // Decode each setting; non-zero is true, zero is false.
      autoBrightnessEnabled = (value[0] != 0);
      accelerometerEnabled = (value[1] != 0);
      sleepModeEnabled = (value[2] != 0);
      auroraModeEnabled = (value[3] != 0);
      if (value.length() >= 5)
      {
        staticColorModeEnabled = (value[4] != 0);
      }
      else
      {
        staticColorModeEnabled = false;
        Serial.println("Static color flag not provided, defaulting to disabled.");
      }

      if (staticColorModeEnabled)
      {
        // Static color mode takes precedence over aurora mode.
        auroraModeEnabled = false;
      }
      setAutoBrightness(autoBrightnessEnabled);
      setAuroraMode(auroraModeEnabled);
      setStaticColorMode(staticColorModeEnabled);
      // Log the updated settings.
      Serial.print("  Auto Brightness: ");
      Serial.println(autoBrightnessEnabled ? "Enabled" : "Disabled");
      Serial.print("  Accelerometer:   ");
      Serial.println(accelerometerEnabled ? "Enabled" : "Disabled");
      Serial.print("  Sleep Mode:      ");
      Serial.println(sleepModeEnabled ? "Enabled" : "Disabled");
      Serial.print("  Aurora Mode:     ");
      Serial.println(auroraModeEnabled ? "Enabled" : "Disabled");
      Serial.print("  Static Color:    ");
      Serial.println(staticColorModeEnabled ? "Enabled" : "Disabled");
      // Apply configuration changes based on the new settings.
      applyConfigOptions();
      if (oldAutoBrightnessEnabled != autoBrightnessEnabled)
      { // If the setting actually changed
        if (autoBrightnessEnabled)
        {
          Serial.println("Auto brightness has been ENABLED. Adaptive logic will take over.");
        }
        else
        {
          Serial.println("Auto brightness has been DISABLED. Applying user-set brightness.");
          dma_display->setBrightness8(userBrightness);
          updateGlobalBrightnessScale(userBrightness);
          lastBrightness = userBrightness;
          currentBrightness = static_cast<float>(userBrightness);
          Serial.printf("Applied manual brightness: %u\n", userBrightness);
        }
      }
      if (oldAuroraMode != auroraModeEnabled || oldStaticColorMode != staticColorModeEnabled)
      {
        facePlasmaDirty = true;
      }
    }
    else
    {
      Serial.println("Error: Config payload is not at least 4 bytes.");
    }
  }
};
// Reuse one instance to avoid heap fragmentation
static ConfigCallbacks configCallbacks;

class BrightnessCallbacks : public NimBLECharacteristicCallbacks
{
  void onWrite(NimBLECharacteristic *pChr, NimBLEConnInfo &connInfo) override
  {
    auto val = pChr->getValue();
    if (val.size() >= 1)
    {
      userBrightness = (uint8_t)val[0]; // Store the app-set brightness
      brightnessChanged = true;         // Flag that brightness was set by user
      setUserBrightness(userBrightness);
      // ADDED: Apply brightness immediately if auto-brightness is off
      if (!autoBrightnessEnabled)
      {
        dma_display->setBrightness8(userBrightness);
        updateGlobalBrightnessScale(userBrightness);
        lastBrightness = userBrightness;
        currentBrightness = static_cast<float>(userBrightness);
        Serial.printf("Applied manual brightness: %u\n", userBrightness);
      }
      Serial.printf("Brightness set to %u\n", userBrightness);
    }
  }
};
static BrightnessCallbacks brightnessCallbacks;

class StaticColorCallbacks : public NimBLECharacteristicCallbacks
{
  void onWrite(NimBLECharacteristic *pChr, NimBLEConnInfo &connInfo) override
  {
    const std::string &val = pChr->getValue();
    if (val.empty())
    {
      return;
    }

    std::string normalized;
    if (!applyStaticColorBytes(reinterpret_cast<const uint8_t *>(val.data()), val.size(), true, &normalized))
    {
      Serial.println("Invalid static color payload received over BLE.");
      return;
    }

    pChr->setValue(normalized);
    pChr->notify();
#if DEBUG_MODE
    Serial.printf("Static color updated to #%s\n", normalized.c_str());
#endif
  }

  void onRead(NimBLECharacteristic *pChr, NimBLEConnInfo &connInfo) override
  {
    String hexString = getStaticColorHexString();
    std::string value(hexString.c_str());
    pChr->setValue(value);
  }
};
static StaticColorCallbacks staticColorCallbacks;

class DescriptorCallbacks : public NimBLEDescriptorCallbacks
{
  void onWrite(NimBLEDescriptor *pDescriptor, NimBLEConnInfo &connInfo) override
  {
    std::string dscVal = pDescriptor->getValue();
    Serial.printf("Descriptor written value: %s\n", dscVal.c_str());
  }
  void onRead(NimBLEDescriptor *pDescriptor, NimBLEConnInfo &connInfo) override
  {
    Serial.printf("%s Descriptor read\n", pDescriptor->getUUID().toString().c_str());
  }
} dscCallbacks;

void handleBLEStatusLED()
{
  const PairingSnapshot pairingSnapshot = getPairingSnapshot();
  if (deviceConnected != oldDeviceConnected)
  {
    bluetoothStatusChangeMillis = millis();
    if (deviceConnected)
    {
      statusPixel.setPixelColor(0, 0, 50, 0); // Green when connected
    }
    else
    {
      statusPixel.setPixelColor(0, 0, 0, 0); // Off when disconnected
      NimBLEDevice::startAdvertising();
    }
    // statusPixel.show();
    oldDeviceConnected = deviceConnected;
  }
  if (!deviceConnected)
  {
    fadeInAndOutLED(0, 0, 100); // Blue fade when disconnected
  }
  if (pairingSnapshot.pairing)
  {
    fadeInAndOutLED(128, 0, 128); // Blue when pairing
  }
  statusPixel.show();
}

static void resetBlePairing()
{
  if (!pServer)
  {
    return;
  }

  setPairingState(false, false, 0, false);
  const std::vector<uint16_t> peers = pServer->getPeerDevices();
  if (!peers.empty())
  {
    setPairingResetPending(true);
    for (uint16_t connHandle : peers)
    {
      pServer->disconnect(connHandle);
    }
    return;
  }

  const bool cleared = NimBLEDevice::deleteAllBonds();
  setPairingResetPending(false);
  NimBLEDevice::startAdvertising();
#if DEBUG_BLE
  Serial.printf("BLE pairing reset: bonds cleared=%s\n", cleared ? "true" : "false");
#endif
}

// Bitmap Drawing Functions ------------------------------------------------
void drawXbm565(int x, int y, int width, int height, const char *xbm, uint16_t color = 0xffff)
{
  // Ensure width is padded to the nearest byte boundary
  int byteWidth = (width + 7) / 8;

  // Pre-check if entire bitmap is out of bounds
  if (x >= dma_display->width() || y >= dma_display->height() ||
      x + width <= 0 || y + height <= 0)
  {
    return; // Completely out of bounds, nothing to draw
  }

  // Calculate visible region to avoid per-pixel boundary checks
  // Use explicit conditionals instead of min/max to avoid overload or macro conflicts.
  int startX = (0 > -x) ? 0 : -x;
  int startY = (0 > -y) ? 0 : -y;
  int tmpEndX = dma_display->width() - x;
  int endX = (width < tmpEndX) ? width : tmpEndX;
  int tmpEndY = dma_display->height() - y;
  int endY = (height < tmpEndY) ? height : tmpEndY;

  for (int j = startY; j < endY; j++)
  {
    uint8_t bitMask = 0x80 >> (startX & 7);        // Start with the correct bit position
    int byteIndex = j * byteWidth + (startX >> 3); // Integer division by 8

    for (int i = startX; i < endX; i++)
    {
      // Check if the bit is set
      if (pgm_read_byte(&xbm[byteIndex]) & bitMask)
      {
        dma_display->drawPixel(x + i, y + j, color);
      }

      // Move to the next bit
      bitMask >>= 1;
      if (bitMask == 0)
      {                 // We've used all bits in this byte
        bitMask = 0x80; // Reset to the first bit of the next byte
        byteIndex++;    // Move to the next byte
      }
    }
  }
}

constexpr uint8_t BLUETOOTH_BACKGROUND_WIDTH = 7;
constexpr uint8_t BLUETOOTH_BACKGROUND_HEIGHT = 11;
constexpr uint8_t BLUETOOTH_RUNE_WIDTH = 5;
constexpr uint8_t BLUETOOTH_RUNE_HEIGHT = 7;
constexpr uint8_t BLUETOOTH_ICON_MARGIN = 1;
constexpr unsigned long BLUETOOTH_FADE_PERIOD_MS = 1600UL;
constexpr unsigned long BLUETOOTH_CONNECTED_FADE_DELAY_MS = 5000UL;
constexpr unsigned long BLUETOOTH_CONNECTED_FADE_DURATION_MS = 1500UL;

static uint8_t scaleColorComponent(uint8_t value, float intensity)
{
  float scaled = value * intensity;
  if (scaled < 0.0f)
  {
    scaled = 0.0f;
  }
  if (scaled > 255.0f)
  {
    scaled = 255.0f;
  }
  return static_cast<uint8_t>(scaled + 0.5f);
}

void drawBluetoothStatusIcon()
{
  if (!dma_display)
  {
    return;
  }

  const unsigned long nowMs = millis();
  const int iconX = dma_display->width() - BLUETOOTH_BACKGROUND_WIDTH - BLUETOOTH_ICON_MARGIN - 64;
  const int iconY = dma_display->height() - BLUETOOTH_BACKGROUND_HEIGHT - BLUETOOTH_ICON_MARGIN;

  float intensity = 1.0f;
  if (!deviceConnected)
  {
    const float normalized = static_cast<float>(nowMs % BLUETOOTH_FADE_PERIOD_MS) / static_cast<float>(BLUETOOTH_FADE_PERIOD_MS);
    const float wave = 0.5f * (sinf(normalized * TWO_PI) + 1.0f); // Range 0..1
    intensity = 0.15f + (0.65f * wave);                           // Keep a faint glow at minimum
  }
  else
  {
    if (bluetoothStatusChangeMillis == 0)
    {
      bluetoothStatusChangeMillis = nowMs;
    }
    const unsigned long connectedDuration = nowMs - bluetoothStatusChangeMillis;
    if (connectedDuration > BLUETOOTH_CONNECTED_FADE_DELAY_MS)
    {
      const unsigned long fadeElapsed = connectedDuration - BLUETOOTH_CONNECTED_FADE_DELAY_MS;
      if (fadeElapsed >= BLUETOOTH_CONNECTED_FADE_DURATION_MS)
      {
        intensity = 0.0f;
      }
      else
      {
        const float fadeProgress = static_cast<float>(fadeElapsed) / static_cast<float>(BLUETOOTH_CONNECTED_FADE_DURATION_MS);
        intensity = 1.0f - fadeProgress;
      }
    }
  }

  const uint16_t backgroundColor = dma_display->color565(
      scaleColorComponent(5, intensity),
      scaleColorComponent(90, intensity),
      scaleColorComponent(180, intensity));

  const uint16_t runeColor = dma_display->color565(
      scaleColorComponent(255, intensity),
      scaleColorComponent(255, intensity),
      scaleColorComponent(255, intensity));

  // Clear the drawing area (pad by 1 px to avoid leftover pixels from other views)
  int clearX = iconX - 1;
  int clearY = iconY - 1;
  int clearW = BLUETOOTH_BACKGROUND_WIDTH + 2;
  int clearH = BLUETOOTH_BACKGROUND_HEIGHT + 2;
  if (clearX < 0)
  {
    clearW += clearX;
    clearX = 0;
  }
  if (clearY < 0)
  {
    clearH += clearY;
    clearY = 0;
  }
  if (clearW > 0 && clearH > 0)
  {
    dma_display->fillRect(clearX, clearY, clearW, clearH, 0);
  }

  drawXbm565(iconX, iconY, BLUETOOTH_BACKGROUND_WIDTH, BLUETOOTH_BACKGROUND_HEIGHT, bluetoothBackground, backgroundColor);
  drawXbm565(iconX + BLUETOOTH_BACKGROUND_WIDTH + 2, iconY, BLUETOOTH_BACKGROUND_WIDTH, BLUETOOTH_BACKGROUND_HEIGHT, bluetoothBackground, backgroundColor);

  const int runeX = iconX + static_cast<int>((BLUETOOTH_BACKGROUND_WIDTH - BLUETOOTH_RUNE_WIDTH) / 2);
  const int runeY = iconY + static_cast<int>((BLUETOOTH_BACKGROUND_HEIGHT - BLUETOOTH_RUNE_HEIGHT) / 2);

  drawXbm565(runeX, runeY, BLUETOOTH_RUNE_WIDTH, BLUETOOTH_RUNE_HEIGHT, bluetoothRune, runeColor);
  drawXbm565(runeX + BLUETOOTH_RUNE_WIDTH + 4, runeY, BLUETOOTH_RUNE_WIDTH, BLUETOOTH_RUNE_HEIGHT, bluetoothRune, runeColor);
}

static void drawPairingPasskeyOverlay()
{
  if (!dma_display)
  {
    return;
  }

  const PairingSnapshot pairingSnapshot = getPairingSnapshot();
  if (!pairingSnapshot.pairing || !pairingSnapshot.passkeyValid)
  {
    return;
  }

  static bool labelMetricsCached = false;
  static int16_t labelX1 = 0;
  static int16_t labelY1 = 0;
  static uint16_t labelW = 0;
  static uint16_t labelH = 0;
  static bool keyMetricsCached = false;
  static uint32_t cachedPasskey = 0;
  static int16_t keyX1 = 0;
  static int16_t keyY1 = 0;
  static uint16_t keyW = 0;
  static uint16_t keyH = 0;

  const uint32_t passkey = pairingSnapshot.passkey;
  char passkeyStr[7];
  snprintf(passkeyStr, sizeof(passkeyStr), "%06lu", static_cast<unsigned long>(passkey));

  const char *label = "PASSKEY";

  if (!labelMetricsCached)
  {
    dma_display->setFont(&TomThumb);
    dma_display->setTextSize(1);
    dma_display->getTextBounds(label, 0, 0, &labelX1, &labelY1, &labelW, &labelH);
    labelMetricsCached = true;
  }

  if (!keyMetricsCached || passkey != cachedPasskey)
  {
    dma_display->setFont(&FreeSans9pt7b);
    dma_display->setTextSize(1);
    dma_display->getTextBounds(passkeyStr, 0, 0, &keyX1, &keyY1, &keyW, &keyH);
    cachedPasskey = passkey;
    keyMetricsCached = true;
  }

  const int padding = 2;
  const int gap = 1;
  const int contentW = (labelW > keyW) ? labelW : keyW;
  const int boxW = contentW + (padding * 2);
  const int boxH = labelH + keyH + gap + (padding * 2);

  const int screenW = dma_display->width();
  const int screenH = dma_display->height();
  int boxX = (screenW - boxW) / 2;
  int boxY = (screenH - boxH) / 2;

  if (boxX < 0)
  {
    boxX = 0;
  }
  if (boxY < 0)
  {
    boxY = 0;
  }

  dma_display->clearScreen();
  const uint16_t backgroundColor = dma_display->color565(0, 0, 0);
  const uint16_t borderColor = dma_display->color565(255, 255, 255);
  dma_display->fillRect(boxX, boxY, boxW, boxH, backgroundColor);
  dma_display->drawRect(boxX, boxY, boxW, boxH, borderColor);

  const int labelLeft = boxX + (boxW - labelW) / 2;
  const int labelTop = boxY + padding;
  const int labelCursorX = labelLeft - labelX1;
  const int labelCursorY = labelTop - labelY1;

  dma_display->setFont(&TomThumb);
  dma_display->setTextSize(1);
  dma_display->setTextColor(borderColor);
  dma_display->setCursor(labelCursorX, labelCursorY);
  dma_display->print(label);

  const int keyLeft = boxX + (boxW - keyW) / 2;
  const int keyTop = labelTop + labelH + gap;
  const int keyCursorX = keyLeft - keyX1;
  const int keyCursorY = keyTop - keyY1;

  dma_display->setFont(&FreeSans9pt7b);
  dma_display->setTextSize(1);
  dma_display->setTextColor(borderColor);
  dma_display->setCursor(keyCursorX, keyCursorY);
  dma_display->print(passkeyStr);
}

uint16_t colorWheel(uint8_t pos)
{
  if (pos < 85)
  {
    return dma_display->color565(pos * 3, 255 - pos * 3, 0);
  }
  else if (pos < 170)
  {
    pos -= 85;
    return dma_display->color565(255 - pos * 3, 0, pos * 3);
  }
  pos -= 170;
  return dma_display->color565(0, pos * 3, 255 - pos * 3);
}

void initializeScrollingText()
{
  if (!dma_display)
  {
    return;
  }

  dma_display->setFont(&FreeSans9pt7b);
  dma_display->setTextSize(1);
  dma_display->setTextWrap(false);

  int16_t x1 = 0;
  int16_t y1 = 0;
  uint16_t w = 0;
  uint16_t h = 0;
  dma_display->getTextBounds(txt, 0, 0, &x1, &y1, &w, &h);

  textX = dma_display->width();
  textY = ((dma_display->height() - h) / 2) - y1;
  textMin = -static_cast<int16_t>(w + SCROLL_TEXT_GAP);

  auto &scrollState = scroll::state();
  scroll::resetTiming();
  const unsigned long now = millis();
  scrollState.lastScrollTickMs = now;
  scrollState.lastBackgroundTickMs = now;
  scrollState.backgroundOffset = 0;
  scrollState.colorOffset = 0;
  scrollState.textInitialized = true;
}

static void drawScrollingBackground(uint8_t offset)
{
  if (!dma_display)
  {
    return;
  }

  const int width = dma_display->width();
  const int height = dma_display->height();

  dma_display->clearScreen();
  for (int y = 0; y < height; ++y)
  {
    const uint8_t paletteIndex = sin8(static_cast<uint8_t>(y * 8) + offset);
    const CRGB color = ColorFromPalette(CloudColors_p, paletteIndex);
    dma_display->drawFastHLine(0, y, width, dma_display->color565(color.r, color.g, color.b));
  }
}

static void renderScrollingTextView()
{
  auto &scrollState = scroll::state();
  if (!scrollState.textInitialized)
  {
    initializeScrollingText();
  }

  const unsigned long now = millis();

  if (now - scrollState.lastBackgroundTickMs >= SCROLL_BACKGROUND_INTERVAL_MS)
  {
    scrollState.lastBackgroundTickMs = now;
    ++scrollState.backgroundOffset;
  }

  // drawScrollingBackground(scrollState.backgroundOffset);

  if (now - scrollState.lastScrollTickMs >= scrollState.textIntervalMs)
  {
    scrollState.lastScrollTickMs = now;
    --textX;
    if (textX <= textMin)
    {
      textX = dma_display->width() + SCROLL_TEXT_GAP;
    }
    scrollState.colorOffset = static_cast<uint8_t>(scrollState.colorOffset + 3);
  }

  drawText(scrollState.colorOffset);
}

uint16_t plasmaSpeed = 2; // Lower = slower animation

void drawPlasmaXbm(int x, int y, int width, int height, const char *xbm,
                   uint8_t time_offset = 0, float scale = 5.0f, float animSpeed = 0.2f,
                   uint8_t brightnessScale = 255)
{
  const int byteWidth = (width + 7) >> 3;
  if (byteWidth <= 0)
  {
    return;
  }

  const bool useStaticColorMode = staticColorModeEnabled;
  const uint16_t combinedBrightnessScale = static_cast<uint16_t>(
      (static_cast<uint32_t>(globalBrightnessScaleFixed) * static_cast<uint16_t>(brightnessScale) + 128u) >> 8);
  uint16_t staticColor565 = 0;
  if (useStaticColorMode)
  {
    ensureStaticColorLoaded();
    CRGB color = gStaticColorState.color;
    color.r = static_cast<uint8_t>((static_cast<uint16_t>(color.r) * combinedBrightnessScale + 128) >> 8);
    color.g = static_cast<uint8_t>((static_cast<uint16_t>(color.g) * combinedBrightnessScale + 128) >> 8);
    color.b = static_cast<uint8_t>((static_cast<uint16_t>(color.b) * combinedBrightnessScale + 128) >> 8);
    staticColor565 = dma_display->color565(color.r, color.g, color.b);
  }

  const uint16_t scaleFixed = static_cast<uint16_t>(scale * 256.0f);
  if (scaleFixed == 0)
  {
    return;
  }
  const uint16_t scaleHalfFixed = scaleFixed >> 1;
  int32_t startXFixed = static_cast<int32_t>(x) * static_cast<int32_t>(scaleFixed);

  const uint16_t animSpeedFixed = static_cast<uint16_t>(animSpeed * 256.0f);
  const uint32_t effectiveTimeFixed = static_cast<uint32_t>(time_counter) * animSpeedFixed;
  const uint8_t t = static_cast<uint8_t>(effectiveTimeFixed >> 8);
  const uint8_t t2 = static_cast<uint8_t>(t >> 1);
  const uint8_t t3 = static_cast<uint8_t>(((effectiveTimeFixed / 3U) >> 8) & 0xFF);

  const uint16_t brightnessScaleFixed = combinedBrightnessScale;

  for (int j = 0; j < height; ++j)
  {
    const int yj = y + j;
    const int32_t yScaleFixed = static_cast<int32_t>(yj) * scaleFixed;
    const uint8_t cos_val = cos8(static_cast<uint8_t>((yScaleFixed >> 8) + t2));
    int32_t tempFixed = static_cast<int32_t>(x + yj) * scaleHalfFixed;
    int32_t xFixed = startXFixed;
    const uint8_t *rowPtr = reinterpret_cast<const uint8_t *>(xbm) + j * byteWidth;

    for (int i = 0; i < width; ++i)
    {
      const int byteIndex = i >> 3;
      const uint8_t bitMask = static_cast<uint8_t>(1U << (7 - (i & 7)));

      if (rowPtr[byteIndex] & bitMask)
      {
        if (useStaticColorMode)
        {
          dma_display->drawPixel(x + i, yj, staticColor565);
        }
        else
        {
          const uint8_t sin_val = sin8(static_cast<uint8_t>((xFixed >> 8) + t));
          const uint8_t sin_val2 = sin8(static_cast<uint8_t>((tempFixed >> 8) + t3));
          const uint8_t v = sin_val + cos_val + sin_val2;

          const uint8_t paletteIndex = static_cast<uint8_t>(v + time_offset);
          CRGB color = ColorFromPalette(currentPalette, paletteIndex);
          const uint16_t scale = brightnessScaleFixed;
          color.r = static_cast<uint8_t>((static_cast<uint16_t>(color.r) * scale + 128) >> 8);
          color.g = static_cast<uint8_t>((static_cast<uint16_t>(color.g) * scale + 128) >> 8);
          color.b = static_cast<uint8_t>((static_cast<uint16_t>(color.b) * scale + 128) >> 8);

          dma_display->drawPixel(x + i, yj, dma_display->color565(color.r, color.g, color.b));
        }
      }

      xFixed += scaleFixed;
      tempFixed += scaleHalfFixed;
    }
  }
}

void drawText(int colorWheelOffset)
{
  // Set text properties
  dma_display->setFont(&FreeSans9pt7b);
  dma_display->setTextSize(1);

  // Use the color wheel for text color
  uint16_t textColor = colorWheel(colorWheelOffset);
  dma_display->setTextColor(textColor);

  // Set cursor and print text
  dma_display->setCursor(textX, textY);
  dma_display->print(txt);
}

// NEW function to draw bitmap with blink squash effect
void drawBitmapWithBlink(int x, int y, int width, int height, const uint8_t *bitmap, uint16_t color, int progress)
{
  int byteWidth = (width + 7) / 8;
  float center_y = (height - 1) / 2.0f;

  // This formula replicates the "w" calculation from the emulator for the squash effect
  float w = 0.005f + (1.0f - 0.005f) * (progress / 100.0f);

  for (int j = 0; j < height; j++)
  {
    for (int i = 0; i < width; i++)
    {
      if (pgm_read_byte(&bitmap[j * byteWidth + i / 8]) & (0x80 >> (i % 8)))
      {
        // Calculate brightness based on vertical distance from center, modulated by 'w'
        float blinkBrightness = pow(2, -w * pow(j - center_y, 2));
        if (blinkBrightness < 0.01)
          continue;

        // Apply the main color modulated by the blink brightness
        uint8_t r = (color >> 11) & 0x1F;
        uint8_t g = (color >> 5) & 0x3F;
        uint8_t b = color & 0x1F;

        r = r * blinkBrightness;
        g = g * blinkBrightness;
        b = b * blinkBrightness;

        dma_display->drawPixel(x + i, y + j, dma_display->color565(r << 3, g << 2, b << 3));
      }
    }
  }
}

// NEW ADVANCED function to draw a bitmap with optional plasma and blink squash effects
void drawBitmapAdvanced(int x, int y, int width, int height, const uint8_t *bitmap,
                        uint16_t color, int progress, bool usePlasma,
                        uint8_t time_offset = 0, float scale = 5.0, float animSpeed = 0.2f)
{
  int byteWidth = (width + 7) / 8;
  float center_y = (height - 1) / 2.0f;

  // --- Blink Effect Calculation ---
  float w = 0.005f + (1.0f - 0.005f) * (progress / 100.0f);

  const bool enablePlasma = usePlasma && !staticColorModeEnabled;
  CRGB staticColorValue;
  if (staticColorModeEnabled)
  {
    ensureStaticColorLoaded();
    staticColorValue = gStaticColorState.color;
  }

  // --- Plasma Effect Setup (if enabled) ---
  uint8_t t = 0, t2 = 0, t3 = 0;
  float scaleHalf = 0;
  if (enablePlasma)
  {
    float effectiveTimeFloat = time_counter * animSpeed;
    t = (uint8_t)effectiveTimeFloat;
    t2 = (uint8_t)(effectiveTimeFloat / 2);
    t3 = (uint8_t)(effectiveTimeFloat / 3);
    scaleHalf = scale * 0.5f;
  }

  for (int j = 0; j < height; j++)
  {
    // --- OPTIMIZATION: Calculate blink brightness once per row ---
    float blinkBrightness = powf(2.0f, -w * powf(j - center_y, 2));
    // If the entire row is too dim to be visible, skip it completely.
    if (blinkBrightness < 0.01f)
      continue;

    // Pre-calculate plasma values that are constant for the row
    float y_val_plasma = (y + j) * scale;
    float tempSum_plasma = (x + y + j) * scaleHalf;
    float x_val_plasma = x * scale;
    uint8_t cos_val_plasma = 0;
    if (enablePlasma)
    {
      cos_val_plasma = cos8(y_val_plasma + t2);
    }

    for (int i = 0; i < width; i++)
    {
      // Check if the pixel in the bitmap is set
      if (pgm_read_byte(&bitmap[j * byteWidth + i / 8]) & (0x80 >> (i % 8)))
      {

        CRGB final_color;

        if (enablePlasma)
        {
          // --- Plasma Color Calculation ---
          uint8_t sin_val = sin8(x_val_plasma + t);
          uint8_t sin_val2 = sin8(tempSum_plasma + i * scaleHalf + t3);
          uint8_t v = sin_val + cos_val_plasma + sin_val2;
          final_color = ColorFromPalette(currentPalette, v + time_offset);
        }
        else if (staticColorModeEnabled)
        {
          final_color = staticColorValue;
        }
        else
        {
          // --- Solid Color Calculation ---
          uint8_t r = (color >> 11) & 0x1F;
          uint8_t g = (color >> 5) & 0x3F;
          uint8_t b = color & 0x1F;
          final_color = CRGB((r * 255) / 31, (g * 255) / 63, (b * 255) / 31);
        }

        // Apply blink brightness and global brightness scale
        final_color.r = (uint8_t)(final_color.r * blinkBrightness * globalBrightnessScale);
        final_color.g = (uint8_t)(final_color.g * blinkBrightness * globalBrightnessScale);
        final_color.b = (uint8_t)(final_color.b * blinkBrightness * globalBrightnessScale);

        dma_display->drawPixel(x + i, y + j, dma_display->color565(final_color.r, final_color.g, final_color.b));
      }
      // This must remain in the inner loop
      if (enablePlasma)
      {
        x_val_plasma += scale;
      }
    }
  }
}

// NEW: Update idle eye hover animation
void updateIdleHoverAnimation()
{
  const uint32_t currentMs = millis();

  const int prevIdleYOffset = idleEyeYOffset;
  const int prevIdleXOffset = idleEyeXOffset;

  if (IDLE_HOVER_PERIOD_MS_Y > 0)
  {
    const uint32_t phaseY = (static_cast<uint64_t>(currentMs % IDLE_HOVER_PERIOD_MS_Y) << 16) / IDLE_HOVER_PERIOD_MS_Y;
    const int32_t sinY = sin16(static_cast<uint16_t>(phaseY));
    idleEyeYOffset = static_cast<int>((sinY * IDLE_HOVER_AMPLITUDE_Y + 16384) >> 15);
  }
  else
  {
    idleEyeYOffset = 0;
  }

  if (IDLE_HOVER_PERIOD_MS_X > 0)
  {
    const uint32_t phaseX = (static_cast<uint64_t>(currentMs % IDLE_HOVER_PERIOD_MS_X) << 16) / IDLE_HOVER_PERIOD_MS_X;
    const int32_t cosX = cos16(static_cast<uint16_t>(phaseX));
    idleEyeXOffset = static_cast<int>((cosX * IDLE_HOVER_AMPLITUDE_X + 16384) >> 15);
  }
  else
  {
    idleEyeXOffset = 0;
  }

  if (idleEyeYOffset != prevIdleYOffset || idleEyeXOffset != prevIdleXOffset)
  {
    facePlasmaDirty = true;
  }
}

// NEW: Update eye bounce animation (Modified for multiple bounces)
void updateEyeBounceAnimation()
{
  int newOffset = currentEyeYOffset;

  if (!isEyeBouncing)
  {
    eyeBounceCount = 0;
    if (newOffset != 0)
    {
      newOffset = 0;
      facePlasmaDirty = true;
    }
    currentEyeYOffset = newOffset;
    return;
  }

  unsigned long elapsed = millis() - eyeBounceStartTime;

  if (elapsed >= EYE_BOUNCE_DURATION)
  {
    eyeBounceCount++;
    if (eyeBounceCount >= MAX_EYE_BOUNCES)
    {
      isEyeBouncing = false;
      newOffset = 0;
      eyeBounceCount = 0;

      if (currentView == VIEW_CIRCLE_EYES)
      {
        currentView = viewBeforeEyeBounce;
        saveLastView(currentView);
        LOG_DEBUG("Eye bounce finished. Reverting to view: %d\n", currentView);
        notifyBleTask();
      }

      if (currentEyeYOffset != newOffset)
      {
        facePlasmaDirty = true;
      }
      currentEyeYOffset = newOffset;
      return;
    }
    else
    {
      eyeBounceStartTime = millis();
      elapsed = 0;
    }
  }

  const unsigned long halfDuration = EYE_BOUNCE_DURATION / 2;
  if (elapsed < halfDuration)
  {
    newOffset = map(elapsed, 0, halfDuration, 0, EYE_BOUNCE_AMPLITUDE);
  }
  else
  {
    newOffset = map(elapsed, halfDuration, EYE_BOUNCE_DURATION, EYE_BOUNCE_AMPLITUDE, 0);
  }

  if (newOffset != currentEyeYOffset)
  {
    currentEyeYOffset = newOffset;
    facePlasmaDirty = true;
  }
}

void drawPlasmaFace()
{
  // Keep idle hover offsets in sync even if the main loop stalls
  updateIdleHoverAnimation();

  // Combine bounce and idle hover offsets
  // Combine bounce and idle hover offsets
  int final_y_offset = currentEyeYOffset + idleEyeYOffset;
  int final_x_offset_right = idleEyeXOffset; // X offset for the right eye (0,0 based)
  int final_x_offset_left = idleEyeXOffset;  // X offset for the left eye (96,0 based)

  // Draw eyes with different plasma parameters
  int y_offset = currentEyeYOffset;                                                    // Get current bounce offset for eyes
  drawPlasmaXbm(0 + final_x_offset_right, 0 + final_y_offset, 32, 16, Eye, 0, 1.0);    // Right eye
  drawPlasmaXbm(96 + final_x_offset_left, 0 + final_y_offset, 32, 16, EyeL, 128, 1.0); // Left eye (phase offset)
  // Nose with finer pattern
  drawPlasmaXbm(56, 14, 8, 8, nose, 64, 2.0);
  drawPlasmaXbm(64, 14, 8, 8, noseL, 64, 2.0);
  // Mouth with larger pattern

  // drawPlasmaXbm(0, 16, 64, 16, maw, 32, 2.0);
  // drawPlasmaXbm(64, 16, 64, 16, mawL, 32, 2.0);

  const uint8_t mouthBrightness = micGetMouthBrightness();
  drawPlasmaXbm(0, 10, 64, 22, maw2Closed, 32, 0.5f, 0.2f, mouthBrightness);
  drawPlasmaXbm(64, 10, 64, 22, maw2ClosedL, 32, 0.5f, 0.2f, mouthBrightness);
}

void updatePlasmaFace()
{
  static unsigned long lastUpdate = 0;
  static unsigned long lastPaletteChange = 0;
  const uint16_t frameDelay = 2;               // Delay for plasma animation update
  const unsigned long paletteInterval = 10000; // Change palette every 10 seconds
  unsigned long now = millis();

  if (now - lastUpdate >= frameDelay)
  {
    lastUpdate = now;
    time_counter += plasmaSpeed; // Update plasma animation counter
    facePlasmaDirty = true;
  }

  if (now - lastPaletteChange > paletteInterval)
  {
    lastPaletteChange = now;
    currentPalette = palettes[random(0, sizeof(palettes) / sizeof(palettes[0]))];
    facePlasmaDirty = true;
  }
}

// SETUP - RUNS ONCE AT PROGRAM START --------------------------------------

void displayLoadingBar()
{
  // dma_display->clearScreen();

  // Draw the loading bar background
  int barWidth = dma_display->width() - 80;               // Width of the loading bar
  int barHeight = 5;                                      // Height of the loading bar
  int barX = (dma_display->width() / 4) - (barWidth / 2); // Center X position
  int barY = (dma_display->height() - barHeight) / 2;     // Center vertically
  // int barRadius = 4;                // Rounded corners
  dma_display->drawRect(barX, (barY * 2), barWidth, barHeight, dma_display->color565(9, 9, 9));
  dma_display->drawRect(barX + 64, (barY * 2), barWidth, barHeight, dma_display->color565(9, 9, 9));

  // Draw the loading progress
  int progressWidth = (barWidth - 2) * loadingProgress / loadingMax;
  dma_display->fillRect(barX + 1, (barY * 2) + 1, progressWidth, barHeight - 2, dma_display->color565(255, 255, 255));
  dma_display->fillRect((barX + 1) + 64, (barY * 2) + 1, progressWidth, barHeight - 2, dma_display->color565(255, 255, 255));
}

void initBlinkState()
{
  blinkState.startTime = 0;
  blinkState.durationMs = 700;
  blinkState.closeDuration = 50;
  blinkState.holdDuration = 20;
  blinkState.openDuration = 50;
}

void updateBlinkAnimationOld()
{
  static bool initialized = false;
  if (!initialized)
  {
    initBlinkState();
    initialized = true;
  }
  const unsigned long now = millis();

  if (isBlinking)
  {
    const unsigned long elapsed = now - blinkState.startTime;
    const unsigned long closeDur = blinkState.closeDuration;
    const unsigned long holdDur = blinkState.holdDuration;
    const unsigned long openDur = blinkState.openDuration;
    const unsigned long totalDuration = closeDur + holdDur + openDur;

    // Safety and validity checks: terminate blink if too much time has elapsed or if phase durations are invalid.
    if (elapsed > blinkState.durationMs * 2 || totalDuration == 0)
    {
      isBlinking = false;
      setBlinkProgress(0);
      lastEyeBlinkTime = now;
      nextBlinkDelay = random(minBlinkDelay, maxBlinkDelay);
      return;
    }

    // Pre-calculate phase thresholds.
    const unsigned long phase1End = closeDur;           // End of closing phase.
    const unsigned long phase2End = closeDur + holdDur; // End of hold phase.

    if (elapsed < phase1End)
    {
      // Closing phase with integer ease-in.
      const unsigned long scaledProgress = (elapsed * 100UL) / closeDur;
      const unsigned long eased = (scaledProgress * scaledProgress + 50UL) / 100UL;
      setBlinkProgress(static_cast<int>(eased > 100UL ? 100UL : eased));
    }
    else if (elapsed < phase2End)
    {
      // Hold phase – full blink.
      setBlinkProgress(100);
    }
    else if (elapsed < totalDuration)
    {
      // Opening phase with integer ease-out.
      const unsigned long openElapsed = elapsed - phase2End;
      const unsigned long scaledProgress = (openElapsed * 100UL) / openDur;
      const unsigned long inv = (scaledProgress > 100UL) ? 0UL : (100UL - scaledProgress);
      const unsigned long eased = 100UL - ((inv * inv + 50UL) / 100UL);
      setBlinkProgress(static_cast<int>(eased > 100UL ? 100UL : eased));
    }
    else
    {
      // Blink cycle complete; reset variables.
      isBlinking = false;
      setBlinkProgress(0);
      lastEyeBlinkTime = now;
      nextBlinkDelay = random(minBlinkDelay, maxBlinkDelay);
    }
  }
  else if (now - lastEyeBlinkTime >= nextBlinkDelay)
  {
    isBlinking = true;
    blinkState.startTime = now;
    blinkState.durationMs = random(minBlinkDuration, maxBlinkDuration);
    blinkState.closeDuration = blinkState.durationMs * 0.30;
    blinkState.holdDuration = blinkState.durationMs * 0.15;
    blinkState.openDuration = blinkState.durationMs * 0.55;
    blinkProgress = 0;
  }
}

void updateBlinkAnimation()
{
  const unsigned long now = millis();

  // Check if it's time to start a new blink
  if (manualBlinkTrigger || (!isBlinking && (now - lastEyeBlinkTime >= nextBlinkDelay)))
  {
    isBlinking = true;
    manualBlinkTrigger = false;
    blinkState.startTime = now;

    // Randomize total blink duration and phase ratios, like in the emulator
    unsigned long totalDuration = 400 + random(301); // 400 to 700 ms
    float closeRatio = 0.2f + (random(21) / 100.0f); // 0.2 to 0.4
    float holdRatio = 0.05f + (random(11) / 100.0f); // 0.05 to 0.15

    blinkState.closeDuration = totalDuration * closeRatio;
    blinkState.holdDuration = totalDuration * holdRatio;
    blinkState.openDuration = totalDuration * (1.0f - closeRatio - holdRatio);
    blinkProgress = 0;
  }

  // If a blink is in progress, update its state
  if (isBlinking)
  {
    const unsigned long elapsed = now - blinkState.startTime;
    const unsigned long totalDuration = blinkState.closeDuration + blinkState.holdDuration + blinkState.openDuration;

    if (elapsed < blinkState.closeDuration)
    {
      float t = (float)elapsed / blinkState.closeDuration;
      blinkProgress = 100 * easeInQuad(t);
    }
    else if (elapsed < blinkState.closeDuration + blinkState.holdDuration)
    {
      blinkProgress = 100;
    }
    else if (elapsed < totalDuration)
    {
      float t = (float)(elapsed - blinkState.closeDuration - blinkState.holdDuration) / blinkState.openDuration;
      blinkProgress = 100 * (1.0f - easeOutQuad(t));
    }
    else
    {
      // Blink finished
      isBlinking = false;
      blinkProgress = 0;
      lastEyeBlinkTime = now;
      // Calculate delay for the next blink
      nextBlinkDelay = minBlinkDelay + random(maxBlinkDelay - minBlinkDelay + 1);
      // 15% chance of a quick "double blink"
      if (random(100) < 15)
      {
        nextBlinkDelay = 100 + random(151);
      }
    }
  }
}

// Optimized rotation function
void drawRotatedBitmap(int16_t x, int16_t y, const uint8_t *bitmap,
                       uint16_t w, uint16_t h, float cosA, float sinA,
                       uint16_t color)
{
  const int16_t cx = w / 2;
  const int16_t cy = h / 2;
  const int byteWidth = (w + 7) >> 3;

  const int32_t cosFixed = static_cast<int32_t>(cosA * 65536.0f);
  const int32_t sinFixed = static_cast<int32_t>(sinA * 65536.0f);

  for (uint16_t j = 0; j < h; ++j)
  {
    const int16_t dy = static_cast<int16_t>(j) - cy;
    int32_t baseXFixed = (static_cast<int32_t>(-cx) * cosFixed) - (static_cast<int32_t>(dy) * sinFixed);
    int32_t baseYFixed = (static_cast<int32_t>(-cx) * sinFixed) + (static_cast<int32_t>(dy) * cosFixed);

    baseXFixed += static_cast<int32_t>(x) << 16;
    baseYFixed += static_cast<int32_t>(y) << 16;

    int32_t pixelXFixed = baseXFixed;
    int32_t pixelYFixed = baseYFixed;
    uint8_t currentByte = 0;

    for (uint16_t i = 0; i < w; ++i)
    {
      if ((i & 7U) == 0U)
      {
        currentByte = pgm_read_byte(&bitmap[j * byteWidth + (i >> 3)]);
      }

      if (currentByte & (0x80U >> (i & 7U)))
      {
        const int16_t newX = static_cast<int16_t>(pixelXFixed >> 16);
        const int16_t newY = static_cast<int16_t>(pixelYFixed >> 16);

        if (newX >= 0 && newX < PANE_WIDTH && newY >= 0 && newY < PANE_HEIGHT)
        {
          dma_display->drawPixel(newX, newY, color);
        }
      }

      pixelXFixed += cosFixed;
      pixelYFixed += sinFixed;
    }
  }
}

void updateRotatingSpiral()
{
  static unsigned long lastUpdate = 0;
  static float currentAngle = 0;
  const unsigned long rotationFrameInterval = 11; // ~90 FPS (11ms per frame)
  unsigned long currentTime = millis();

  if (currentTime - lastUpdate < rotationFrameInterval)
    return;
  lastUpdate = currentTime;

  // Calculate color once per frame
  static uint8_t colorIndex = 0;
  colorIndex += 2; // Slower color transition
  CRGB currentColor = ColorFromPalette(currentPalette, colorIndex);
  uint16_t color = dma_display->color565(currentColor.r, currentColor.g, currentColor.b);

  // Optimized rotation with pre-calculated values
  float radians = currentAngle * PI / 180.0;
  float cosA = cos(radians);
  float sinA = sin(radians);
  currentAngle = fmod(currentAngle + 4, 360); // Slower rotation (4° per frame)

  // Clear previous spiral frame
  // dma_display->fillRect(24, 15, 32, 25, 0);
  // dma_display->fillRect(105, 15, 32, 25, 0);
  //  Draw both spirals using pre-transformed coordinates
  drawRotatedBitmap(24, 15, spiral, 25, 25, cosA, sinA, color);
  drawRotatedBitmap(105, 15, spiralL, 25, 25, cosA, sinA, color);
}

// Draw the blinking eyes
void blinkingEyes()
{
  // Combine bounce and idle hover offsets
  int final_y_offset = currentEyeYOffset + idleEyeYOffset;
  int final_x_offset = idleEyeXOffset;

  // --- Eye Configuration ---
  const uint8_t *rightEyeBitmap = (const uint8_t *)slanteyes; // Default
  const uint8_t *leftEyeBitmap = (const uint8_t *)slanteyesL; // Default
  int eyeWidth = 24, eyeHeight = 16;
  int rightEyeX = 2, rightEyeY = 0;
  int leftEyeX = 102, leftEyeY = 0;
  bool usePlasma = true;                                    // Most views use plasma
  uint16_t solidColor = dma_display->color565(0, 255, 255); // Default cyan, not used if usePlasma is true

  // Select eye assets and properties based on the current view
  switch (currentView)
  {
  case VIEW_NORMAL_FACE: // Normal
  case VIEW_BLUSH_FACE: // Blush
    rightEyeBitmap = (const uint8_t *)Eye;
    leftEyeBitmap = (const uint8_t *)EyeL;
    eyeWidth = 32;
    eyeHeight = 16;
    rightEyeX = 0;
    rightEyeY = 0;
    leftEyeX = 96;
    leftEyeY = 0;
    break;
  case VIEW_SEMICIRCLE_EYES: // Semicircle
    rightEyeBitmap = (const uint8_t *)semicircleeyes;
    leftEyeBitmap = (const uint8_t *)semicircleeyes; // Same bitmap, different plasma offset
    eyeWidth = 32;
    eyeHeight = 12;
    rightEyeX = 2;
    rightEyeY = 2;
    leftEyeX = 94;
    leftEyeY = 2;
    break;
  case VIEW_X_EYES: // X eyes
    rightEyeBitmap = (const uint8_t *)x_eyes;
    leftEyeBitmap = (const uint8_t *)x_eyes; // Same bitmap
    eyeWidth = 31;
    eyeHeight = 15;
    rightEyeX = 0;
    rightEyeY = 0;
    leftEyeX = 96;
    leftEyeY = 0;
    break;
  case VIEW_SLANT_EYES: // Slant eyes (This is also the default)
    // Values are already set by default
    break;
  case VIEW_SPIRAL_EYES:   // Spiral eyes
    return; // Spiral view handles its own drawing, so we exit here.
  case VIEW_CIRCLE_EYES:  // Circle eyes
    rightEyeBitmap = (const uint8_t *)circleeyes;
    leftEyeBitmap = (const uint8_t *)circleeyes; // Same bitmap
    eyeWidth = 25;
    eyeHeight = 21;
    rightEyeX = 10;
    rightEyeY = 2;
    leftEyeX = 93;
    leftEyeY = 2;
    break;
    // Default case uses the slanteyes defined before the switch
  }

  // Draw the right eye (viewer's perspective)
  drawBitmapAdvanced(rightEyeX + final_x_offset, rightEyeY + final_y_offset,
                     eyeWidth, eyeHeight, rightEyeBitmap, solidColor,
                     blinkProgress, usePlasma, 0);

  // Draw the left eye with a plasma phase offset if plasma is used
  drawBitmapAdvanced(leftEyeX + final_x_offset, leftEyeY + final_y_offset,
                     eyeWidth, eyeHeight, leftEyeBitmap, solidColor,
                     blinkProgress, usePlasma, 128);
}

// Function to disable/clear the blush display when the effect is over
void disableBlush()
{
  Serial.println("Blush disabled!");
  facePlasmaDirty = true; // Defer actual redraw to display task
}

// Update the blush effect state (non‑blocking)
void updateBlush()
{
  unsigned long now = millis();
  unsigned long elapsed = now - blushStateStartTime;

  switch (blushState)
  {
  case BlushState::FadeIn:
    if (elapsed < fadeInDuration)
    {
      // Use ease-in for a smooth start
      float progress = (float)elapsed / fadeInDuration;
      blushBrightness = 255 * easeInQuad(progress);
    }
    else
    {
      blushBrightness = 255;
      blushState = BlushState::Full;
      blushStateStartTime = now; // Restart timer for full brightness phase
    }
    break;

  case BlushState::Full:
    if (elapsed >= fullDuration)
    {
      // After full brightness time, start fading out
      blushState = BlushState::FadeOut;
      blushStateStartTime = now; // Restart timer for fade-out
    }
    // Brightness remains at 255 during this phase.
    break;

  case BlushState::FadeOut:
    if (elapsed < fadeOutDuration)
    {
      // Use ease-out for a smooth end
      float progress = (float)elapsed / fadeOutDuration;
      blushBrightness = 255 * (1.0f - easeOutQuad(progress));
    }
    else
    {
      blushBrightness = 0;
      blushState = BlushState::Inactive;
      disableBlush();

      // Revert to previous view if this was an overlay blush
      if (wasBlushOverlay)
      {
        currentView = originalViewBeforeBlush;
        saveLastView(currentView);
        Serial.printf("Blush overlay finished. Reverting to view: %d\n", currentView);
        notifyBleTask();
        wasBlushOverlay = false; // Reset the flag
      }
    }
    break;

  default:
    wasBlushOverlay = false; // Ensure flag is reset if state becomes inactive
    break;
  }
}

void drawBlush()
{
  // Serial.print("Blush brightness: ");
  // Serial.println(blushBrightness);

  // Set blush color based on brightness
  uint16_t blushColor = dma_display->color565(blushBrightness, 0, blushBrightness);

  const int blushWidth = 11;
  const int blushHeight = 13;
  const int blushCount = 3;
  const int blushY = 1;
  const int rightStartX = 23;
  const int leftStartX = 72;
  const int totalBlushWidth = blushWidth * blushCount;

  // Clear only the blush area to prevent artifacts
  dma_display->fillRect(rightStartX, blushY, totalBlushWidth, blushHeight, 0);
  dma_display->fillRect(leftStartX, blushY, totalBlushWidth, blushHeight, 0);

  for (int i = 0; i < blushCount; ++i)
  {
    drawXbm565(rightStartX + (i * blushWidth), blushY, blushWidth, blushHeight, blush, blushColor);
    drawXbm565(leftStartX + (i * blushWidth), blushY, blushWidth, blushHeight, blushL, blushColor);
  }
}

void drawTransFlag()
{
  // dma_display->clearScreen(); // Clear the display

  int stripeHeight = dma_display->height() / 5; // Height of each stripe

  // Define colors in RGB565 format with intended values:
  // lightBlue: (0,51,102), pink: (255,10,73), white: (255,255,255)
  uint16_t lightBlue = dma_display->color565(0, (102 / 2), (204 / 2));
  uint16_t pink = dma_display->color565(255, (20 / 2), (147 / 2));
  uint16_t white = dma_display->color565(255, 255, 255);

  // Draw stripes
  dma_display->fillRect(0, 0, dma_display->width(), stripeHeight, lightBlue);                // Top light blue stripe
  dma_display->fillRect(0, stripeHeight, dma_display->width(), stripeHeight, pink);          // Pink stripe
  dma_display->fillRect(0, stripeHeight * 2, dma_display->width(), stripeHeight, white);     // Middle white stripe
  dma_display->fillRect(0, stripeHeight * 3, dma_display->width(), stripeHeight, pink);      // Pink stripe
  dma_display->fillRect(0, stripeHeight * 4, dma_display->width(), stripeHeight, lightBlue); // Bottom light blue stripe
}

void drawLGBTFlag()
{
  int stripeHeight = dma_display->height() / 6; // Height of each stripe

  // Define colors in RGB565 format with intended values:
  // red: (255,0,0), orange: (255,127,0), yellow: (255,255,0), green: (0,255,0), blue: (0,0,255), purple: (139,0,255)
  uint16_t red = dma_display->color565(255, 0, 0);
  uint16_t orange = dma_display->color565(255, 127, 0);
  uint16_t yellow = dma_display->color565(255, 255, 0);
  uint16_t green = dma_display->color565(0, 255, 0);
  uint16_t blue = dma_display->color565(0, 0, 255);
  uint16_t purple = dma_display->color565(139, 0, 255);

  // Draw stripes
  dma_display->fillRect(0, 0, dma_display->width(), stripeHeight, red);          // Top red stripe
  dma_display->fillRect(0, stripeHeight, dma_display->width(), stripeHeight, orange);  // Orange stripe
  dma_display->fillRect(0, stripeHeight * 2, dma_display->width(), stripeHeight, yellow); // Yellow stripe
  dma_display->fillRect(0, stripeHeight * 3, dma_display->width(), stripeHeight, green);  // Green stripe
  dma_display->fillRect(0, stripeHeight * 4, dma_display->width(), stripeHeight, blue);   // Blue stripe
  dma_display->fillRect(0, stripeHeight * 5, dma_display->width(), stripeHeight, purple); // Bottom purple stripe
}

void baseFace()
{
  // Update idle hover here so the render task always sees fresh offsets
  updateIdleHoverAnimation();

  // Combine all offsets for final positioning.
  // These will be used for any facial feature that should move with the hover/bounce effect.
  int final_y_offset = currentEyeYOffset + idleEyeYOffset;
  int final_x_offset = idleEyeXOffset;

  blinkingEyes(); // This function now correctly uses the global offsets internally

  const uint8_t mouthBrightness = micGetMouthBrightness();
  if (mouthOpen)
  {
    drawPlasmaXbm(0, 10, 64, 22, maw2, 0, 1.0f, 0.2f, mouthBrightness);
    drawPlasmaXbm(64, 10, 64, 22, maw2L, 128, 1.0f, 0.2f, mouthBrightness); // Left eye open (phase offset)
  }
  else
  {
    drawPlasmaXbm(0, 10, 64, 22, maw2Closed, 0, 1.0f, 0.2f, mouthBrightness);     // Right eye
    drawPlasmaXbm(64, 10, 64, 22, maw2ClosedL, 128, 1.0f, 0.2f, mouthBrightness); // Left eye (phase offset)
  }

  drawPlasmaXbm(56, 4 + final_y_offset, 8, 8, nose, 64, 2.0);
  drawPlasmaXbm(64, 4 + final_y_offset, 8, 8, noseL, 64, 2.0);

  facePlasmaDirty = false;
}

void staticColor()
{
  ensureStaticColorLoaded();
  const CRGB color = gStaticColorState.color;
  const uint16_t encodedColor = dma_display->color565(color.r, color.g, color.b);
  dma_display->fillScreen(encodedColor);
}

void patternPlasma()
{
  // Advance plasma animation timing + palette cycling reused by all plasma-based views
  updatePlasmaFace();

  static uint16_t paletteLUT[256];
  static CRGBPalette16 cachedPalette;
  static uint16_t cachedBrightnessScale = 0;
  static bool paletteLUTValid = false;

  if (!paletteLUTValid || cachedBrightnessScale != globalBrightnessScaleFixed || std::memcmp(&cachedPalette, &currentPalette, sizeof(CRGBPalette16)) != 0)
  {
    const uint16_t scale = globalBrightnessScaleFixed;
    for (int i = 0; i < 256; ++i)
    {
      CRGB color = ColorFromPalette(currentPalette, static_cast<uint8_t>(i));
      color.r = static_cast<uint8_t>((static_cast<uint16_t>(color.r) * scale + 128) >> 8);
      color.g = static_cast<uint8_t>((static_cast<uint16_t>(color.g) * scale + 128) >> 8);
      color.b = static_cast<uint8_t>((static_cast<uint16_t>(color.b) * scale + 128) >> 8);
      paletteLUT[i] = dma_display->color565(color.r, color.g, color.b);
    }
    cachedPalette = currentPalette;
    cachedBrightnessScale = scale;
    paletteLUTValid = true;
  }

  // Pre-calculate values that only depend on time_counter
  // These are constant for the entire frame draw
  const uint8_t wibble = sin8(time_counter);
  // Pre-calculate the cosine term and the division by 8 (as shift)
  // Note: cos8 argument is uint8_t, so -time_counter wraps around correctly.
  const uint8_t cos8_val_shifted = cos8(static_cast<uint8_t>(-time_counter)) >> 3; // Calculate cos8(-t)/8 once
  const uint16_t time_val = time_counter;                                          // Use a local copy for calculations
  const uint16_t term2_factor = 128 - wibble;                                      // Calculate 128 - sin8(t) once

  // Get display dimensions once
  const int display_width = dma_display->width();
  const int display_height = dma_display->height();

  // Outer loop for X
  for (int x = 0; x < display_width; x++)
  {
    // Pre-calculate terms dependent only on X and time
    const uint16_t term1_base = x * wibble * 3 + time_val;
    // The y*x part needs to be inside the Y loop, but we can precalculate x * cos8_val_shifted
    const uint16_t term3_x_factor = x * cos8_val_shifted;

    // Inner loop for Y
    for (int y = 0; y < display_height; y++)
    {
      // Calculate plasma value 'v'
      // Start with base offset
      int16_t v = 128;
      // Add terms using pre-calculated values where possible
      v += sin16(term1_base);                  // sin16(x*wibble*3 + t)
      v += cos16(y * term2_factor + time_val); // cos16(y*(128-wibble) + t)
      v += sin16(y * term3_x_factor);          // sin16(y * (x * cos8(-t) >> 3))

      const uint16_t color565 = paletteLUT[static_cast<uint8_t>(v >> 8)];
      dma_display->drawPixel(x, y, color565);
    }
  }
}

void displaySleepMode()
{
  static unsigned long lastBlinkTime = 0;
  static bool eyesOpen = false;
  static float animationPhase = 0;
  static unsigned long lastAnimTime = 0;

  // Apply brightness adjustment for breathing effect
  static unsigned long lastBreathTime = 0;
  static float brightness = 0;
  static float breathingDirection = 1; // 1 for increasing, -1 for decreasing

  // Update breathing effect
  if (millis() - lastBreathTime >= 50)
  {
    lastBreathTime = millis();

    // Update breathing brightness
    brightness += breathingDirection * 0.01; // Slow breathing effect

    // Reverse direction at limits
    if (brightness >= 1.0)
    {
      brightness = 1.0;
      breathingDirection = -1;
    }
    else if (brightness <= 0.1)
    { // Keep a minimum brightness
      brightness = 0.1;
      breathingDirection = 1;
    }

    // Apply breathing effect to overall brightness
    uint8_t currentBrightness = sleepBrightness * brightness;
    dma_display->setBrightness8(currentBrightness);
    updateGlobalBrightnessScale(currentBrightness);
  }

  dma_display->clearScreen();

  // Simple animation for floating Zs
  if (millis() - lastAnimTime > 100)
  {
    animationPhase += 0.1;
    lastAnimTime = millis();
  }

  // Draw animated ZZZs with offset based on animation phase
  int offset1 = sin8(animationPhase * 20) / 16;
  int offset2 = sin8((animationPhase + 0.3) * 20) / 16;
  int offset3 = sin8((animationPhase + 0.6) * 20) / 16;

  drawXbm565(27, 2 + offset1, 8, 8, sleepZ1, dma_display->color565(150, 150, 150));
  drawXbm565(37, 0 + offset2, 8, 8, sleepZ2, dma_display->color565(100, 100, 100));
  drawXbm565(47, 1 + offset3, 8, 8, sleepZ3, dma_display->color565(50, 50, 50));

  drawXbm565(71, 2 + offset1, 8, 8, sleepZ1, dma_display->color565(50, 50, 50));
  drawXbm565(81, 0 + offset2, 8, 8, sleepZ2, dma_display->color565(100, 100, 100));
  drawXbm565(91, 1 + offset3, 8, 8, sleepZ3, dma_display->color565(150, 150, 150));

  /*
  // Draw closed or slightly open eyes
  if (millis() - lastBlinkTime > 4000)
  { // Occasional slow blink
    eyesOpen = !eyesOpen;
    lastBlinkTime = millis();
  }
*/
  // Draw nose
  drawXbm565(56, 14, 8, 8, nose, dma_display->color565(100, 100, 100));
  drawXbm565(64, 14, 8, 8, noseL, dma_display->color565(100, 100, 100));

  if (eyesOpen)
  {
    // Draw slightly open eyes - just a small slit
    dma_display->fillRect(5, 5, 20, 2, dma_display->color565(150, 150, 150));
    dma_display->fillRect(103, 5, 20, 2, dma_display->color565(150, 150, 150));
  }
  else
  {
    // Draw closed eyes
    dma_display->drawLine(5, 10, 40, 12, dma_display->color565(150, 150, 150));
    // dma_display->drawLine(83, 10, 40, 12, dma_display->color565(150, 150, 150));
    dma_display->drawLine(88, 12, 123, 10, dma_display->color565(150, 150, 150));
  }
  // Draw sleeping mouth (slight curve)
  /*
  drawXbm565(0, 20, 10, 8, maw2Closed, dma_display->color565(120, 120, 120));
  drawXbm565(64, 20, 10, 8, maw2ClosedL, dma_display->color565(120, 120, 120));
  */
  dma_display->drawFastHLine(PANE_WIDTH / 2 - 15, PANE_HEIGHT - 8, 30, dma_display->color565(120, 120, 120));
  drawBluetoothStatusIcon();
  drawPairingPasskeyOverlay();
  dma_display->flipDMABuffer();
}

void initStarfield()
{
  for (int i = 0; i < NUM_STARS; i++)
  {
    stars[i].x = random(0, dma_display->width());
    stars[i].y = random(0, dma_display->height());
    stars[i].speed = random(1, 4); // Stars move at different speeds
  }
}

void updateStarfield()
{
  // Clear the display for a fresh frame
  dma_display->clearScreen(); // Uncommented to clear the screen
  // Update and draw each star
  for (int i = 0; i < NUM_STARS; i++)
  {
    dma_display->drawPixel(stars[i].x, stars[i].y, dma_display->color565(255, 255, 255));
    // Move star leftwards based on its speed
    stars[i].x -= stars[i].speed;
    // Reset star to right edge if it goes off the left side
    if (stars[i].x < 0)
    {
      stars[i].x = dma_display->width() - 1;
      stars[i].y = random(0, dma_display->height());
      stars[i].speed = random(1, 4);
    }
  }
  // dma_display->flipDMABuffer();
}

void initDVDLogoAnimation()
{
  logos[0] = {0, 0, 1, 1, dma_display->color565(255, 0, 255)};   // Left panel
  logos[1] = {64, 0, -1, 1, dma_display->color565(0, 255, 255)}; // Right panel
}

void bleNotifyTask(void *param)
{
  for (;;)
  {
    // block here until someone calls xTaskNotifyGive()
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
#if DEBUG_MODE
    const uint32_t bleStartMicros = micros();
#endif
    if (deviceConnected)
    {
      uint8_t bleViewValue = static_cast<uint8_t>(currentView);
      pFaceCharacteristic->setValue(&bleViewValue, 1);
      pFaceCharacteristic->notify();
      pConfigCharacteristic->notify();
    }
#if DEBUG_MODE
    const uint32_t bleElapsed = micros() - bleStartMicros;
    if (bleElapsed > SLOW_SECTION_THRESHOLD_US)
    {
      Serial.printf("BLE notify slow: %lu us\n", static_cast<unsigned long>(bleElapsed));
    }
#endif
  }
}

// Dedicated rendering task keeps DMA flips on a steady cadence.
static void displayTask(void *pvParameters)
{
  (void)pvParameters;
  const TickType_t frameTicks = pdMS_TO_TICKS(targetFrameIntervalMillis);
  TickType_t lastWake = xTaskGetTickCount();
#if DEBUG_MODE
  static unsigned long lastSlowFrameLogMs = 0;
#endif

  for (;;)
  {
#if DEBUG_MODE
    const uint32_t frameStartMicros = micros();
#endif
    displayCurrentView(currentView);
#if DEBUG_MODE
    const uint32_t frameMicros = micros() - frameStartMicros;
    gLastFrameDurationMicros = frameMicros;
    if (frameMicros > gWorstFrameDurationMicros)
    {
      gWorstFrameDurationMicros = frameMicros;
    }
    if (frameMicros > SLOW_FRAME_THRESHOLD_US)
    {
      const unsigned long now = millis();
      if (now - lastSlowFrameLogMs >= 250)
      {
        Serial.printf("Slow frame %lu us (view %d, worst %lu us)\n", static_cast<unsigned long>(frameMicros), currentView, static_cast<unsigned long>(gWorstFrameDurationMicros));
        lastSlowFrameLogMs = now;
      }
    }
#endif
    vTaskDelayUntil(&lastWake, frameTicks);
  }
}



static volatile unsigned long softMillis = 0; // our software “millis” counter
TimerHandle_t softMillisTimer = nullptr;

// this callback fires every 1 ms and increments softMillis:
static void IRAM_ATTR onSoftMillisTimer(TimerHandle_t xTimer)
{
  (void)xTimer;
  softMillis++;
}

// Matrix and Effect Configuration
#define MAX_FPS 45 // Maximum redraw rate, frames/second

#ifndef PDUST_N_COLORS // Allow overriding from a global config if needed
#define PDUST_N_COLORS 8
#endif

#define N_COLORS PDUST_N_COLORS // Number of distinct colors for sand
#define BOX_HEIGHT 4            // Initial height of each color block of sand

// Calculate total number of sand grains.
// Logic: Each color forms a block. There are N_COLORS blocks.
// Width of each block = WIDTH / N_COLORS
// Number of grains per block = (WIDTH / N_COLORS) * BOX_HEIGHT
// Total grains = N_COLORS * (WIDTH / N_COLORS) * BOX_HEIGHT = WIDTH * BOX_HEIGHT
// Original calculation: (BOX_HEIGHT * N_COLORS * 8). If WIDTH=64, N_COLORS=8, BOX_HEIGHT=8,
// this is (8 * 8 * 8) = 512. And WIDTH * BOX_HEIGHT = 64 * 8 = 512. So they are equivalent for these values.
#define N_GRAINS (PANE_WIDTH * BOX_HEIGHT) // Total number of sand grains
// #define N_GRAINS 10
// #include "pixelDustEffect.h" // Include the header for this effect
// #include "main.h"            // Include your main project header if it exists (e.g., for global configs)

// Define global variables declared as extern in the header
uint16_t colors[N_COLORS];
Adafruit_PixelDust sand(PANE_WIDTH, PANE_HEIGHT, N_GRAINS, 1.0f, 128, false); // elasticity 1.0, accel_scale 128
uint32_t prevTime = 0;
static bool pixelDustInitialized = false;

// Initializes the Pixel Dust effect settings
void setupPixelDust()
{
  if (!dma_display)
  {
    Serial.println("PixelDust Error: dma_display is not initialized before setupPixelDust() call!");
    return;
  }
  if (!pixelDustInitialized)
  {
    if (!sand.begin())
    {
      Serial.println("PixelDust Error: sand.begin() failed!");
      return;
    }
    pixelDustInitialized = true;
  }
  if (N_COLORS <= 0)
  {
    Serial.println("PixelDust Error: N_COLORS must be greater than zero.");
    return;
  }

  sand.clear();
  sand.randomize();

  static const uint16_t defaultPalette[] = {
      dma_display->color565(64, 64, 64),
      dma_display->color565(120, 79, 23),
      dma_display->color565(228, 3, 3),
      dma_display->color565(255, 140, 0),
      dma_display->color565(255, 237, 0),
      dma_display->color565(0, 128, 38),
      dma_display->color565(0, 77, 255),
      dma_display->color565(117, 7, 135)
    };

  const size_t paletteCount = sizeof(defaultPalette) / sizeof(defaultPalette[0]);
  // If N_COLORS exceeds the size of defaultPalette, random colors will be used for the extra entries.
  for (int i = 0; i < N_COLORS; ++i)
  {
    // Use default palette color if available, otherwise generate a random color
    uint16_t color;
    if (i < static_cast<int>(paletteCount)) {
      color = defaultPalette[i];
    } else {
      // Use a color wheel to ensure distinct fallback colors
      uint8_t hue = (i * 256 / N_COLORS) % 256;
      CRGB rgb;
      hsv2rgb_rainbow(CHSV(hue, 255, 255), rgb);
      color = dma_display->color565(rgb.r, rgb.g, rgb.b);
    }
    colors[i] = color;
  }
}



// Add an enum for clarity (optional, but good practice)
enum SpiralColorMode
{
  SPIRAL_COLOR_PALETTE,
  SPIRAL_COLOR_WHITE
};

// Modified function signature:
void updateAndDrawFullScreenSpiral(SpiralColorMode colorMode)
{ // Added colorMode parameter
  unsigned long currentTime = millis();
  if (currentTime - lastFullScreenSpiralUpdateTime < FULL_SCREEN_SPIRAL_FRAME_INTERVAL_MS)
  {
    return;
  }
  lastFullScreenSpiralUpdateTime = currentTime;

  // Update animation parameters (rotation, color scroll - color scroll only if palette mode)
  fullScreenSpiralAngle += FULL_SPIRAL_ROTATION_SPEED_RAD_PER_UPDATE;
  if (fullScreenSpiralAngle >= TWO_PI)
    fullScreenSpiralAngle -= TWO_PI;

  if (colorMode == SPIRAL_COLOR_PALETTE)
  { // Only update color offset if using palette
    fullScreenSpiralColorOffset += FULL_SPIRAL_COLOR_SCROLL_SPEED;
  }

  const int centerX = PANE_WIDTH / 2;
  const int centerY = PANE_HEIGHT / 2;
  const float maxRadiusSquared = powf(hypotf(PANE_WIDTH / 2.0f, PANE_HEIGHT / 2.0f) * 1.1f, 2.0f);
  const float deltaTheta = 0.05f;
  const float growthFactor = expf(LOG_SPIRAL_B_COEFF * deltaTheta);
  const float rotCos = cosf(deltaTheta);
  const float rotSin = sinf(deltaTheta);

  float radius = LOG_SPIRAL_A_COEFF;
  float sinAngle = sinf(fullScreenSpiralAngle);
  float cosAngle = cosf(fullScreenSpiralAngle);

  float x = radius * cosAngle;
  float y = radius * sinAngle;

  float colorPhase = fullScreenSpiralColorOffset;
  const float colorPhaseStep = deltaTheta * SPIRAL_ARM_COLOR_FACTOR;

  const int maxSteps = 512;
  for (int step = 0; step < maxSteps; ++step)
  {
    const int drawX = static_cast<int>(centerX + x + 0.5f);
    const int drawY = static_cast<int>(centerY + y + 0.5f);

    uint16_t pixel_color;
    if (colorMode == SPIRAL_COLOR_WHITE)
    {
      pixel_color = dma_display->color565(255, 255, 255);
    }
    else
    {
      const uint8_t color_index = static_cast<uint8_t>(colorPhase);
      CRGB crgb_color = ColorFromPalette(RainbowColors_p, color_index, 255, LINEARBLEND);
      pixel_color = dma_display->color565(crgb_color.r, crgb_color.g, crgb_color.b);
    }

    if (SPIRAL_THICKNESS_RADIUS == 0)
    {
      if (drawX >= 0 && drawX < PANE_WIDTH && drawY >= 0 && drawY < PANE_HEIGHT)
      {
        dma_display->drawPixel(drawX, drawY, pixel_color);
      }
    }
    else
    {
      const int side_length = SPIRAL_THICKNESS_RADIUS * 2 + 1;
      const int start_x = drawX - SPIRAL_THICKNESS_RADIUS;
      const int start_y = drawY - SPIRAL_THICKNESS_RADIUS;
      dma_display->fillRect(start_x, start_y, side_length, side_length, pixel_color);
    }

    colorPhase += colorPhaseStep;

    const float rotatedX = x * rotCos - y * rotSin;
    const float rotatedY = x * rotSin + y * rotCos;
    x = rotatedX * growthFactor;
    y = rotatedY * growthFactor;

    if ((x * x + y * y) > maxRadiusSquared)
    {
      break;
    }
  }
}

void setup()
{
  Serial.begin(BAUD_RATE);
  delay(500); // Delay for serial monitor to start

  Serial.println(" ");
  Serial.println(" ");
  Serial.println("*****************************************************");
  Serial.println("*****************************************************");
  Serial.println("*****************************************************");
  Serial.println("*******************    LumiFur    *******************");
  Serial.println("*****************************************************");
  Serial.println("*****************************************************");
  Serial.println("*****************************************************");
  Serial.println(" ");
  Serial.println(" ");

  updateGlobalBrightnessScale(userBrightness);

#if DEBUG_MODE
  Serial.println("DEBUG MODE ENABLED");
  Serial.printf("Firmware: %s\n", FIRMWARE_VERSION);
  Serial.printf("Commit: %s\n", GIT_COMMIT);
  Serial.printf("Branch: %s\n", GIT_BRANCH);
  Serial.printf("Build: %s %s\n", BUILD_DATE, BUILD_TIME);
#endif

  initTempSensor(); // Initialize Temperature Sensor

  initPreferences(); // Initialize Preferences
  autoBrightnessEnabled = getAutoBrightness();
  auroraModeEnabled = getAuroraMode();
  staticColorModeEnabled = getStaticColorMode();
  if (staticColorModeEnabled)
  {
    auroraModeEnabled = false;
  }
  ensureStaticColorLoaded();
  {
    String storedText = getUserText();
    if (storedText.length() > 0)
    {
      size_t copyLen = storedText.length();
      if (copyLen >= sizeof(txt))
      {
        copyLen = sizeof(txt) - 1;
      }
      memcpy(txt, storedText.c_str(), copyLen);
      txt[copyLen] = '\0';
      auto &scrollState = scroll::state();
      scrollState.textInitialized = false;
    }
    else
    {
      txt[0] = '\0';
    }

    auto &scrollState = scroll::state();
    scrollState.speedSetting = getScrollSpeed();
    scroll::updateIntervalFromSpeed();
  }

  // - LOAD LAST VIEW -
  currentView = getLastView();
  previousView = currentView;
  Serial.printf("Loaded last view: %d\n", currentView);

  // Retrieve and print stored brightness value.
  // int userBrightness = getUserBrightness();
  // Map userBrightness (1-100) to hardware brightness (1-255).
  // int hwBrightness = map(userBrightness, 1, 100, 1, 255);
  Serial.printf("Stored brightness: %d\n", userBrightness);

  /*
  // --- Initialize Scrolling Text ---
    dma_display->setFont(&FreeSans9pt7b); // Set font to measure text width
    dma_display->setTextSize(1);
    strcpy(txt, "LumiFur Controller - FW v" FIRMWARE_VERSION);
    int16_t x1, y1;
    uint16_t w, h;
    dma_display->getTextBounds(txt, 0, 0, &x1, &y1, &w, &h);
    textMin = -w;
    textX = dma_display->width();
    textY = (dma_display->height() - h) / 2 + h; // Center vertically
  */

  ///////////////////// Setup BLE ////////////////////////
  Serial.println("Initializing BLE...");
  // NimBLEDevice::init("LumiFur_Controller");
  NimBLEDevice::init("LF-052618");
  // NimBLEDevice::setPower(ESP_PWR_LVL_P9); // Power level 9 (highest) for best range
  // NimBLEDevice::setPower(ESP_PWR_LVL_P21, NimBLETxPowerType::All); // Power level 21 (highest) for best range
  NimBLEDevice::setPower(ESP_PWR_LVL_P21, NimBLETxPowerType::All); // Power level 21 (highest) for best range
  // NimBLEDevice::setSecurityAuth(BLE_SM_PAIR_AUTHREQ_BOND | BLE_SM_PAIR_AUTHREQ_SC);
  NimBLEDevice::setSecurityAuth(true, true, true);
  // Keep default passkey so onPassKeyDisplay supplies a dynamic code.
  NimBLEDevice::setSecurityPasskey(123456);
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_ONLY);
  pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(&serverCallbacks);

  NimBLEService *pService = pServer->createService(SERVICE_UUID);

  NimBLECharacteristic *pDeviceInfoCharacteristic = pService->createCharacteristic(
      INFO_CHARACTERISTIC_UUID,
      NIMBLE_PROPERTY::READ |
          NIMBLE_PROPERTY::NOTIFY);

  // Construct JSON string
  std::string jsonInfo = std::string("{") +
                         "\"fw\":\"" + std::string(FIRMWARE_VERSION) + "\"," +
                         "\"commit\":\"" + std::string(GIT_COMMIT) + "\"," +
                         "\"branch\":\"" + std::string(GIT_BRANCH) + "\"," +
                         "\"build\":\"" + std::string(BUILD_DATE) + " " + BUILD_TIME + "\"," +
                         "\"model\":\"" + std::string(DEVICE_MODEL) + "\"," +
                         "\"compat\":\"" + std::string(APP_COMPAT_VERSION) + "\"," +
                         "\"id\":\"" + NimBLEDevice::getAddress().toString() + "\"" +
                         "}";

  pDeviceInfoCharacteristic->setValue(jsonInfo.c_str());
  Serial.println("Device Info Service started");
  Serial.println(jsonInfo.c_str());

  // Face control characteristic with encryption
  pFaceCharacteristic = pService->createCharacteristic(
      CHARACTERISTIC_UUID,
      NIMBLE_PROPERTY::READ |
          NIMBLE_PROPERTY::WRITE |
          NIMBLE_PROPERTY::WRITE_ENC |
          NIMBLE_PROPERTY::NOTIFY
      // NIMBLE_PROPERTY::READ_ENC  // only allow reading if paired / encrypted
      // NIMBLE_PROPERTY::WRITE_ENC  // only allow writing if paired / encrypted
  );

  // Set initial view value
  uint8_t bleViewValue = static_cast<uint8_t>(currentView);
  pFaceCharacteristic->setValue(&bleViewValue, 1);
  pFaceCharacteristic->setCallbacks(&chrCallbacks);

  pCommandCharacteristic = pService->createCharacteristic(
      COMMAND_CHARACTERISTIC_UUID,
      NIMBLE_PROPERTY::WRITE |
      NIMBLE_PROPERTY::WRITE_ENC |
      NIMBLE_PROPERTY::NOTIFY
    );
  pCommandCharacteristic->setCallbacks(&cmdCallbacks);
  Serial.print("Command Characteristic created, UUID: ");
  Serial.println(pCommandCharacteristic->getUUID().toString().c_str());
  Serial.print("Properties: ");
  Serial.println(pCommandCharacteristic->getProperties()); // Print properties as integer

  // Temperature characteristic with encryption
  pTemperatureCharacteristic =
      pService->createCharacteristic(
          TEMPERATURE_CHARACTERISTIC_UUID,
              NIMBLE_PROPERTY::WRITE |
              NIMBLE_PROPERTY::NOTIFY
          // NIMBLE_PROPERTY::READ_ENC
      );

  // Create a characteristic for configuration.
  pConfigCharacteristic = pService->createCharacteristic(
      CONFIG_CHARACTERISTIC_UUID,
      NIMBLE_PROPERTY::READ |
      NIMBLE_PROPERTY::WRITE |
      NIMBLE_PROPERTY::WRITE_ENC |
    //NIMBLE_PROPERTY::WRITE_NR |
      NIMBLE_PROPERTY::NOTIFY);

  // Set the callback to handle writes.
  pConfigCharacteristic->setCallbacks(&configCallbacks);

  // Optionally, set an initial value.
  uint8_t initValue[5] = {
      static_cast<uint8_t>(autoBrightnessEnabled ? 1 : 0),
      static_cast<uint8_t>(accelerometerEnabled ? 1 : 0),
      static_cast<uint8_t>(sleepModeEnabled ? 1 : 0),
      static_cast<uint8_t>(auroraModeEnabled ? 1 : 0),
      static_cast<uint8_t>(staticColorModeEnabled ? 1 : 0)};

  pConfigCharacteristic->setValue(initValue, sizeof(initValue));
  if (pConfigCharacteristic != nullptr)
  {
    Serial.println("Config Characteristic created");
  }
  else
  {
    Serial.println("Error: Failed to create config characteristic.");
  }

  //
  pTemperatureLogsCharacteristic = pService->createCharacteristic(
      TEMPERATURE_LOGS_CHARACTERISTIC_UUID,
      NIMBLE_PROPERTY::NOTIFY);
  // pTemperatureLogsCharacteristic->setCallbacks(&chrCallbacks);
  Serial.println("Command Characteristic created");
  pTemperatureLogsCharacteristic->setCallbacks(&logsCallbacks); // If using new logsCallbacks
  Serial.print("Temperature Logs Characteristic created, UUID: ");
  Serial.println(pTemperatureLogsCharacteristic->getUUID().toString().c_str());
  Serial.print("Properties: ");
  Serial.println(pTemperatureLogsCharacteristic->getProperties()); // Print properties as integer

  // Set up descriptors
  NimBLE2904 *faceDesc = pFaceCharacteristic->create2904();
  faceDesc->setFormat(NimBLE2904::FORMAT_UINT8);
  faceDesc->setUnit(0x2700); // Unit-less number
  faceDesc->setCallbacks(&dscCallbacks);
  // Add user description descriptor
  NimBLEDescriptor *pDesc =
      pFaceCharacteristic->createDescriptor(
          "2901",
          NIMBLE_PROPERTY::READ,
          20);
  pDesc->setValue("Face Control");

  // Assuming pTemperatureCharacteristic is already created:
  NimBLEDescriptor *tempDesc =
      pTemperatureCharacteristic->createDescriptor(
          "2901", // Standard UUID for the Characteristic User Description
          NIMBLE_PROPERTY::READ,
          20 // Maximum length for the descriptor's value
      );
  tempDesc->setValue("Temperature Sensor");
  NimBLE2904 *tempFormat = pTemperatureCharacteristic->create2904();
  tempFormat->setFormat(NimBLE2904::FORMAT_UINT8);
  tempFormat->setUnit(0x272F); // For example, 0x272F might represent degrees Celsius per specific BLE specifications

  pBrightnessCharacteristic = pService->createCharacteristic(
      BRIGHTNESS_CHARACTERISTIC_UUID,
      NIMBLE_PROPERTY::READ |
          // NIMBLE_PROPERTY::NOTIFY |
          NIMBLE_PROPERTY::WRITE |
          NIMBLE_PROPERTY::WRITE_ENC |
          NIMBLE_PROPERTY::WRITE_NR);
  pBrightnessCharacteristic->setCallbacks(&brightnessCallbacks);
  // initialize with current brightness

  pOtaCharacteristic = pService->createCharacteristic(
      OTA_CHARACTERISTIC_UUID,
      NIMBLE_PROPERTY::READ |
          NIMBLE_PROPERTY::WRITE |
          NIMBLE_PROPERTY::WRITE_ENC |
          NIMBLE_PROPERTY::WRITE_NR |
          NIMBLE_PROPERTY::NOTIFY);
  pOtaCharacteristic->setCallbacks(&otaCallbacks);
  NimBLEDescriptor *otaDesc = pOtaCharacteristic->createDescriptor(
      "2901",
      NIMBLE_PROPERTY::READ,
      20);
  otaDesc->setValue("OTA Control");
  pBrightnessCharacteristic->setValue(&userBrightness, 1);

  setupAdaptiveBrightness();

  // Create lux characteristic (read-only with notifications)
  pLuxCharacteristic = pService->createCharacteristic(
      LUX_CHARACTERISTIC_UUID,
      NIMBLE_PROPERTY::READ |
      NIMBLE_PROPERTY::NOTIFY);

  // Initialize with current lux value
  uint16_t initialLux = getAmbientLuxU16();
  uint8_t luxBytes[2] = {
      static_cast<uint8_t>(initialLux & 0xFF),
      static_cast<uint8_t>((initialLux >> 8) & 0xFF)};
  pLuxCharacteristic->setValue(luxBytes, 2);
  // Add descriptor for lux characteristic
  NimBLEDescriptor *luxDesc = pLuxCharacteristic->createDescriptor(
      "2901", // Standard UUID for Characteristic User Description
      NIMBLE_PROPERTY::READ,
      20 // Maximum length for the descriptor's value
  );
  luxDesc->setValue("Ambient Light Sensor");
  NimBLE2904 *luxFormat = pLuxCharacteristic->create2904();
  luxFormat->setFormat(NimBLE2904::FORMAT_UINT16);
  luxFormat->setUnit(0x2731); // 0x2731 is the standard unit for Illuminance (lux)

  pScrollTextCharacteristic = pService->createCharacteristic(
      SCROLL_TEXT_CHARACTERISTIC_UUID,
      NIMBLE_PROPERTY::READ |
      NIMBLE_PROPERTY::WRITE |
      NIMBLE_PROPERTY::WRITE_ENC |     // Require encryption for writes.
      NIMBLE_PROPERTY::NOTIFY);
  pScrollTextCharacteristic->setCallbacks(&scrollTextCallbacks);

  // Set initial value (current text)
  pScrollTextCharacteristic->setValue(txt);

  // Add descriptor
  NimBLEDescriptor *scrollTextDesc = pScrollTextCharacteristic->createDescriptor(
      "2901",
      NIMBLE_PROPERTY::READ,
      20);
  scrollTextDesc->setValue("Scrolling Text");

  Serial.println("Scroll Text Characteristic created");

  pStaticColorCharacteristic = pService->createCharacteristic(
      STATIC_COLOR_CHARACTERISTIC_UUID,
      NIMBLE_PROPERTY::READ_ENC |
      NIMBLE_PROPERTY::WRITE_ENC |
      NIMBLE_PROPERTY::WRITE_NR |
      NIMBLE_PROPERTY::NOTIFY);
  pStaticColorCharacteristic->setCallbacks(&staticColorCallbacks);
  String initialColorHex = getStaticColorHexString();
  pStaticColorCharacteristic->setValue(initialColorHex.c_str());
  NimBLEDescriptor *staticColorDesc = pStaticColorCharacteristic->createDescriptor(
      "2901",
      NIMBLE_PROPERTY::READ,
      20);
  staticColorDesc->setValue("Static Color");
  Serial.println("Static Color Characteristic created");

  // nimBLEService* pBaadService = pServer->createService("BAAD");
  pService->start();

  /** Create an advertising instance and add the services to the advertised data */
  NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->setName("LumiFur_Controller");
  // pAdvertising->setAppearance(0);         // Appearance value (0 is generic)
  pAdvertising->enableScanResponse(true); // Enable scan response to include more data
  pAdvertising->addServiceUUID(pService->getUUID());
  pAdvertising->start();

  Serial.println("BLE setup complete - advertising started");

  // Spawn BLE task on the pinned core (defined by CONFIG_BT_NIMBLE_PINNED_TO_CORE)
  xTaskCreatePinnedToCore(bleNotifyTask, "BLE Task", 4096, NULL, 1, NULL, CONFIG_BT_NIMBLE_PINNED_TO_CORE);

  // Redefine pins if required
  // HUB75_I2S_CFG::i2s_pins _pins={R1, G1, BL1, R2, G2, BL2, CH_A, CH_B, CH_C, CH_D, CH_E, LAT, OE, CLK};
  // HUB75_I2S_CFG mxconfig(PANEL_WIDTH, PANEL_HEIGHT, PANELS_NUMBER);
  HUB75_I2S_CFG::i2s_pins _pins = {R1_PIN, G1_PIN, B1_PIN, R2_PIN, G2_PIN, B2_PIN, A_PIN, B_PIN, C_PIN, D_PIN, E_PIN, LAT_PIN, OE_PIN, CLK_PIN};

  // Module configuration
  HUB75_I2S_CFG mxconfig(
      PANEL_WIDTH,   // module width
      PANEL_HEIGHT,  // module height
      PANELS_NUMBER, // Chain length
      _pins          // Pin mapping
  );

  mxconfig.gpio.e = PIN_E;
  mxconfig.driver = HUB75_I2S_CFG::FM6126A; // for panels using FM6126A chips
 // mxconfig.i2sspeed = HUB75_I2S_CFG::HZ_20M;  // Causes instability if set too high
  mxconfig.clkphase = false;
  mxconfig.double_buff = true; // <------------- Turn on double buffer

#ifndef VIRTUAL_PANE
  dma_display = new MatrixPanel_I2S_DMA(mxconfig);
  dma_display->begin();
  dma_display->setBrightness8(userBrightness);
  updateGlobalBrightnessScale(userBrightness);
  lastBrightness = userBrightness;
  currentBrightness = static_cast<float>(userBrightness);
  initFlameEffect(dma_display);
#else
  chain = new MatrixPanel_I2S_DMA(mxconfig);
  chain->begin();
  chain->setBrightness8(userBrightness);
  updateGlobalBrightnessScale(userBrightness);
  lastBrightness = userBrightness;
  currentBrightness = static_cast<float>(userBrightness);
  // create VirtualDisplay object based on our newly created dma_display object
  matrix = new VirtualMatrixPanel((*chain), NUM_ROWS, NUM_COLS, PANEL_WIDTH, PANEL_HEIGHT, CHAIN_TOP_LEFT_DOWN);
  initFlameEffect(matrix);
#endif

  dma_display->clearScreen();
  dma_display->flipDMABuffer();

  // Memory allocation for LED buffer
  ledbuff = (CRGB *)malloc(NUM_LEDS * sizeof(CRGB));
  if (ledbuff == nullptr)
  {
    Serial.println("Memory allocation for ledbuff failed!");
    err(250); // Call error handler
  }

#if defined(ARDUINO_ADAFRUIT_MATRIXPORTAL_ESP32S3)
  // Initialize onboard components for MatrixPortal ESP32-S3
  statusPixel.begin(); // Initialize NeoPixel status light

  if (!accel.begin(0x19))
  { // Default I2C address for LIS3DH on MatrixPortal
    Serial.println("Could not find accelerometer, check wiring!");
    err(250);                            // Fast blink = I2C error
    g_accelerometer_initialized = false; // Set flag
  }
  else
  {
    Serial.println("Accelerometer found!");
    accel.setRange(LIS3DH_RANGE_4_G); // 4G range is sufficient for motion detection
    accel.setDataRate(LIS3DH_DATARATE_50_HZ);
    accel.setPerformanceMode(LIS3DH_MODE_LOW_POWER);
    g_accelerometer_initialized = true; // Set flag on success // Set to low power mode
  }
#endif

  // setupPixelDust();

#if defined(ARDUINO_ADAFRUIT_MATRIXPORTAL_ESP32S3) || defined(ACCEL_AVAILABLE)
  // Ensure dma_display, accel, and accelerometerEnabled are initialized before this line
  fluidEffectInstance = new FluidEffect(dma_display, &accel, &accelerometerEnabled);
#else
  // Pass nullptr for accelerometer if not available/compiled
  fluidEffectInstance = new FluidEffect(dma_display, nullptr, &accelerometerEnabled);
#endif

  if (!fluidEffectInstance)
  {
    Serial.println("FATAL: Failed to allocate FluidEffect instance!");
    // Handle error appropriately, e.g., by blinking an LED or halting.
    // For now, it will just print "Fluid Error" in displayCurrentView if null.
  }

  // ... rest of your setup() code ...
  // e.g., currentView = getLastView();

  // If the initial view is the fluid effect, call begin()
  if (currentView == VIEW_FLUID_EFFECT && fluidEffectInstance)
  { // Assuming 16 is the fluid effect view
    fluidEffectInstance->begin();
  }

  lastActivityTime = millis(); // Initialize the activity timer for sleep mode

  randomSeed(analogRead(0)); // Seed the random number generator

  // Set sleep timeout based on debug mode
  SLEEP_TIMEOUT_MS = DEBUG_MODE ? SLEEP_TIMEOUT_MS_DEBUG : SLEEP_TIMEOUT_MS_NORMAL;

  // Set initial plasma color palette
  currentPalette = RainbowColors_p;

  snprintf(txt, sizeof(txt), "%s", getUserText().c_str());
  initializeScrollingText();

  ////////Setup Bouncing Squares////////
  myDARK = dma_display->color565(64, 64, 64);
  myWHITE = dma_display->color565(192, 192, 192);
  myRED = dma_display->color565(255, 0, 0);
  myGREEN = dma_display->color565(0, 255, 0);
  myBLUE = dma_display->color565(0, 0, 255);

  colours = {{myDARK, myWHITE, myRED, myGREEN, myBLUE}};

  // Create some random squares
  for (int i = 0; i < numSquares; i++)
  {
    Squares[i].square_size = random(2, 10);
    Squares[i].xpos = random(0, dma_display->width() - Squares[i].square_size);
    Squares[i].ypos = random(0, dma_display->height() - Squares[i].square_size);
    Squares[i].velocityx = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
    Squares[i].velocityy = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);

    int random_num = random(6);
    Squares[i].colour = colours[random_num];
  }

  // Initialize the starfield
  initStarfieldAnimation();
  initDVDLogoAnimation();

  /* ----------------------------------------------------------------------
  Use internal pull-up resistor, as the up and down buttons
  do not have any pull-up resistors connected to them
  and pressing either of them pulls the input low.
  ------------------------------------------------------------------------- */

  micInit();

#if defined(ARDUINO_ADAFRUIT_MATRIXPORTAL_ESP32S3)
#ifdef BUTTON_UP
  pinMode(BUTTON_UP, INPUT_PULLUP);
#endif
#ifdef BUTTON_DOWN
  pinMode(BUTTON_DOWN, INPUT_PULLUP);
#endif
#endif

  // uint8_t wheelval = 0; // Wheel value for color cycling

  //  #if DEBUG_MODE
  while (loadingProgress <= loadingMax)
  {
    // Boot screen
    dma_display->clearScreen();
    dma_display->setTextSize(2);
    dma_display->setTextColor(dma_display->color565(255, 0, 255));
    dma_display->setCursor(0, 10);
    dma_display->print("LumiFur");
    dma_display->setCursor(64, 10);
    dma_display->print("LumiFur");
    dma_display->setTextSize(1);
    dma_display->setCursor(0, 20);
    dma_display->setTextColor(dma_display->color565(255, 255, 255));
    dma_display->setFont(&TomThumb); // Set font
    dma_display->print("Booting...");
    dma_display->setCursor(64, 20);
    dma_display->print("Booting...");
    displayLoadingBar();
    dma_display->flipDMABuffer();
    loadingProgress++;
    delay(15); // ~1.5 s total at loadingMax=100
  }
  // #endif

  // Spawn BLE notify task (you already have):
  xTaskCreatePinnedToCore(
      bleNotifyTask,
      "BLE Task",
      4096,
      NULL,
      1,
      &bleNotifyTaskHandle,
      CONFIG_BT_NIMBLE_PINNED_TO_CORE);

  // Now spawn the display task pinned to the other core so rendering never competes with loop():

  xTaskCreatePinnedToCore(

      displayTask,

      "Display Task",

      16384, // stack bytes

      NULL,

      2, // priority

      NULL,

      0 // pin to core 0

  );

  // Then return—loop() can be left empty or just do background housekeeping

  // create a 1 ms auto‑reloading FreeRTOS timer
  softMillisTimer = xTimerCreate(
      "softMillis",     // name
      pdMS_TO_TICKS(1), // period = 1 ms
      pdTRUE,           // auto‑reload
      nullptr,          // timer ID
      onSoftMillisTimer // callback
  );
  if (softMillisTimer)
  {
    xTimerStart(softMillisTimer, 0);
  }
}

void drawDVDLogo(int x, int y, uint16_t color)
{
  drawXbm565(x, y, dvdWidth, dvdHeight, DvDLogo, color);
}

void updateDVDLogos()
{
  unsigned long now = millis();
  if (now - lastDvdUpdate < dvdUpdateInterval)
    return;
  lastDvdUpdate = now;

  for (int i = 0; i < 2; i++)
  {
    DVDLogo &logo = logos[i];

    // Clear previous
    // dma_display->fillRect(logo.x, logo.y, dvdWidth, dvdHeight, 0);

    // Update position
    logo.x += logo.vx;
    logo.y += logo.vy;

    // Calculate bounds
    int minX = (i == 0) ? 0 : 64;
    int maxX = minX + 64;

    bool bounced = false;

    // Horizontal bounce (stay within 64x32 region)
    if (logo.x < minX)
    {
      logo.x = minX;
      logo.vx *= -1;
      bounced = true;
    }
    else if (logo.x + dvdWidth > maxX)
    {
      logo.x = maxX - dvdWidth;
      logo.vx *= -1;
      bounced = true;
    }

    // Vertical bounce (shared screen height)
    if (logo.y < 0)
    {
      logo.y = 0;
      logo.vy *= -1;
      bounced = true;
    }
    else if (logo.y + dvdHeight > dma_display->height())
    {
      logo.y = dma_display->height() - dvdHeight;
      logo.vy *= -1;
      bounced = true;
    }
    // Change color if any bounce happened
    if (bounced)
    {
      logo.color = dma_display->color565(random(256), random(256), random(256));
    }
    // Draw new logo
    drawXbm565(logo.x, logo.y, dvdWidth, dvdHeight, DvDLogo, logo.color);
  }
  // dma_display->flipDMABuffer(); // Flip the buffer to show the updated logos
  // Rendering cadence is handled by displayTask; avoid delaying here.
}

void displayCurrentMaw()
{
  const uint8_t mawBrightness = micGetMouthBrightness();
  mouthOpen = micIsMouthOpen();
  // build a gray-scale color from current mic level
  uint16_t col = dma_display->color565(mawBrightness,
                                       mawBrightness,
                                       mawBrightness);
  // Draw open or closed based on the mic‐triggered flag
  if (mouthOpen)
  {
#if DEBUG_MODE
    Serial.println("Displaying open maw");
#endif
    drawXbm565(0, 10, 64, 22, maw2, col);
    drawXbm565(64, 10, 64, 22, maw2L, col);
  }
  else
  {
#if DEBUG_MODE
    Serial.println("Displaying Closed maw");
#endif
    drawXbm565(0, 10, 64, 22, maw2Closed, col);
    drawXbm565(64, 10, 64, 22, maw2ClosedL, col);
  }
}
// Runs one frame of the Pixel Dust animation
bool getLatestAcceleration(float &x, float &y, float &z, const unsigned long sampleInterval);
void PixelDustEffect()
{
  static uint32_t lastFrameMicros = 0;

  const uint32_t now = micros();
  if ((now - lastFrameMicros) < (1000000L / MAX_FPS))
  {
    return;
  }
  lastFrameMicros = now;

  if (!pixelDustInitialized)
  {
    setupPixelDust();
    if (!pixelDustInitialized)
    {
      return;
    }
  }

  if (!dma_display)
  {
    return;
  }

  int16_t ax_scaled = 0;
  int16_t ay_scaled = static_cast<int16_t>(0.1f * 128.0f);

  float accelX = 0.0f;
  float accelY = 0.0f;
  float accelZ = 0.0f;

  if (getLatestAcceleration(accelX, accelY, accelZ, MOTION_SAMPLE_INTERVAL_FAST))
  {
    ax_scaled = static_cast<int16_t>(accelX * 1000.0f);
    ay_scaled = static_cast<int16_t>(accelY * 1000.0f);
  }

  sand.iterate(ax_scaled, ay_scaled, 0);

  dimension_t px, py;
  for (grain_count_t i = 0; i < N_GRAINS; ++i)
  {
    sand.getPosition(i, &px, &py);
    int color_index = 0;
    if (N_COLORS > 0)
    {
      color_index = static_cast<int>((static_cast<uint32_t>(px) * N_COLORS) / PANE_WIDTH);
      if (color_index >= N_COLORS)
      {
        color_index = N_COLORS - 1;
      }
    }
    const int px_i = static_cast<int>(px);
    const int py_i = static_cast<int>(py);
    if (px_i >= 0 && py_i >= 0 && px_i < PANE_WIDTH && py_i < PANE_HEIGHT)
    {
      dma_display->drawPixel(px_i, py_i, colors[color_index]);
    }
  }
}


using ViewRenderFunc = void (*)();

static void renderDebugSquaresView()
{
  for (int i = 0; i < numSquares; i++)
  {
    dma_display->fillRect(Squares[i].xpos, Squares[i].ypos, Squares[i].square_size, Squares[i].square_size, Squares[i].colour);

    if (Squares[i].square_size + Squares[i].xpos >= dma_display->width())
    {
      Squares[i].velocityx *= -1;
    }
    else if (Squares[i].xpos <= 0)
    {
      Squares[i].velocityx = abs(Squares[i].velocityx);
    }

    if (Squares[i].square_size + Squares[i].ypos >= dma_display->height())
    {
      Squares[i].velocityy *= -1;
    }
    else if (Squares[i].ypos <= 0)
    {
      Squares[i].velocityy = abs(Squares[i].velocityy);
    }

    Squares[i].xpos += Squares[i].velocityx;
    Squares[i].ypos += Squares[i].velocityy;
  }
}

static void renderLoadingBarView()
{
  drawXbm565(23, 2, 18, 21, appleLogoApple_logo_black, dma_display->color565(255, 255, 255));
  drawXbm565(88, 2, 18, 21, appleLogoApple_logo_black, dma_display->color565(255, 255, 255));

  displayLoadingBar();
  loadingProgress++;
  if (loadingProgress > loadingMax)
  {
    loadingProgress = 0;
  }
}

static void renderFaceWithPlasma()
{
  baseFace();
  updatePlasmaFace();
}

static void renderSpiralEyesView()
{
  updatePlasmaFace();
  baseFace();
  updateRotatingSpiral();
}

static void renderPlasmaFaceView()
{
  drawPlasmaFace();
  updatePlasmaFace();
}

static void renderBsodView()
{
  dma_display->fillScreen(dma_display->color565(0, 0, 255));
  dma_display->setTextColor(dma_display->color565(255, 255, 255));
  dma_display->setTextSize(2);
  dma_display->setCursor(5, 15);
  dma_display->print(":(");
  dma_display->setCursor(69, 15);
  dma_display->print(":(");
}

static void renderFlameEffectView()
{
  updateAndDrawFlameEffect();
}

static void renderFluidEffectView()
{
  if (fluidEffectInstance)
  {
    fluidEffectInstance->updateAndDraw();
  }
  else
  {
    dma_display->fillRect(0, 0, dma_display->width(), dma_display->height(), 0);
    dma_display->setFont(&TomThumb);
    dma_display->setTextSize(1);
    dma_display->setTextColor(dma_display->color565(255, 0, 0));
    dma_display->setCursor(5, 15);
    dma_display->print("Fluid Err");
  }
}

static void renderPixelDustView()
{
  static bool logged = false;
  if (!logged)
  {
    LOG_DEBUG_LN("PixelDust view active");
    logged = true;
  }
  PixelDustEffect();
}

static void renderFullscreenSpiralPalette()
{
  updateAndDrawFullScreenSpiral(SPIRAL_COLOR_PALETTE);
}

static void renderFullscreenSpiralWhite()
{
  updateAndDrawFullScreenSpiral(SPIRAL_COLOR_WHITE);
}

static const ViewRenderFunc VIEW_RENDERERS[TOTAL_VIEWS] = {
    renderDebugSquaresView,        // VIEW_DEBUG_SQUARES
    renderLoadingBarView,          // VIEW_LOADING_BAR
    patternPlasma,                 // VIEW_PATTERN_PLASMA
    drawTransFlag,                 // VIEW_TRANS_FLAG
    drawLGBTFlag,                  // VIEW_LGBT_FLAG
    renderFaceWithPlasma,          // VIEW_NORMAL_FACE
    renderFaceWithPlasma,          // VIEW_BLUSH_FACE
    renderFaceWithPlasma,          // VIEW_SEMICIRCLE_EYES
    renderFaceWithPlasma,          // VIEW_X_EYES
    renderFaceWithPlasma,          // VIEW_SLANT_EYES
    renderSpiralEyesView,          // VIEW_SPIRAL_EYES
    renderPlasmaFaceView,          // VIEW_PLASMA_FACE
    renderFaceWithPlasma,          // VIEW_UWU_EYES
    updateStarfield,               // VIEW_STARFIELD
    renderBsodView,                // VIEW_BSOD
    updateDVDLogos,                // VIEW_DVD_LOGO
    renderFlameEffectView,         // VIEW_FLAME_EFFECT
    renderFluidEffectView,         // VIEW_FLUID_EFFECT
    renderFaceWithPlasma,          // VIEW_CIRCLE_EYES
    renderFullscreenSpiralPalette, // VIEW_FULLSCREEN_SPIRAL_PALETTE
    renderFullscreenSpiralWhite,   // VIEW_FULLSCREEN_SPIRAL_WHITE
    renderScrollingTextView,       // VIEW_SCROLLING_TEXT
    renderDinoGameView,            // VIEW_DINO_GAME
    renderPixelDustView,           // VIEW_PIXEL_DUST
    staticColor                     // VIEW_STATIC_COLOR
};

static_assert(sizeof(VIEW_RENDERERS) / sizeof(ViewRenderFunc) == TOTAL_VIEWS, "View renderer table mismatch");

void displayCurrentView(int view)
{
  static int previousViewLocal = -1; // Track the last active view

  // If we're in sleep mode, don't display the normal view
  if (sleepModeActive)
  {
    displaySleepMode(); // This function handles its own flipDMABuffer or drawing rate
    return;
  }

  mouthOpen = micIsMouthOpen();

  dma_display->clearScreen(); // Clear the display

  if (view != previousViewLocal)
  { // Check if the view has changed
    facePlasmaDirty = true;
    micSetEnabled(viewUsesMic(view));
    if (view == VIEW_BLUSH_FACE)
    {
      // Reset fade logic when entering the blush view
      blushStateStartTime = millis();
      isBlushFadingIn = true;
      blushState = BlushState::FadeIn; // Start fade‑in state
#if DEBUG_MODE
      Serial.println("Entered blush view, resetting fade logic");
#endif
    }
    if (view == VIEW_FLUID_EFFECT)
    { // If switching to Fluid Animation view
      if (fluidEffectInstance)
      {
        fluidEffectInstance->begin(); // Reset fluid effect state
      }
    }
    if (view == VIEW_FLAME_EFFECT)
    {
      initFlameEffect(dma_display);
    }
    if (view == VIEW_PIXEL_DUST)
    {
      setupPixelDust();
    }
    if (view == VIEW_DINO_GAME)
    {
      resetDinoGame(millis());
    }
    previousViewLocal = view;
  }

  ViewRenderFunc renderer = (view >= 0 && view < TOTAL_VIEWS) ? VIEW_RENDERERS[view] : nullptr;
#if DEBUG_MODE
  static unsigned long lastSlowRendererLog = 0;
  const uint32_t rendererStartMicros = micros();
#endif
  if (renderer)
  {
    renderer();
  }

  if (blushState != BlushState::Inactive)
  {
    drawBlush();
  }
#if DEBUG_MODE
  const uint32_t rendererMicros = micros() - rendererStartMicros;
  if (rendererMicros > SLOW_FRAME_THRESHOLD_US)
  {
    const unsigned long now = millis();
    if (now - lastSlowRendererLog >= 500)
    {
      Serial.printf("Slow renderer %lu us (view %d)\n", static_cast<unsigned long>(rendererMicros), view);
      lastSlowRendererLog = now;
    }
  }
#endif

#if DEBUG_FPS_COUNTER

  // --- Begin FPS counter overlay ---
  static unsigned long lastFpsTime = 0;
  static int frameCount = 0;
  frameCount++;
  unsigned long currentTime = millis();
  if (currentTime - lastFpsTime >= 1000)
  {
    fps = frameCount;
    frameCount = 0;
    lastFpsTime = currentTime;
  }
  char fpsText[8];
  sprintf(fpsText, "FPS %d", fps);
  dma_display->setTextSize(1);                                     // Set text size for FPS counter
  dma_display->setFont(&TomThumb);                                 // Use default font
  dma_display->setTextColor(dma_display->color565(255, 255, 255)); // White text
  // Position the counter at a corner (adjust as needed)
  dma_display->setCursor(37, 5); // Adjust position
  dma_display->print(fpsText);
  // --- End FPS counter overlay ---

#endif

  drawBluetoothStatusIcon();
  drawPairingPasskeyOverlay();

  if (!sleepModeActive || currentView == VIEW_TRANS_FLAG || currentView == VIEW_BSOD)
  {
    dma_display->flipDMABuffer();
  }
}

// Helper function to get the current threshold value
float getCurrentThreshold()
{
  return useShakeSensitivity ? SHAKE_THRESHOLD : SLEEP_THRESHOLD;
}

// Add with your other utility functions
// constexpr unsigned long MOTION_SAMPLE_INTERVAL_FAST = 15;  // ms between accel reads when looking for shakes
constexpr unsigned long MOTION_SAMPLE_INTERVAL_SLOW = 120; // ms between reads for sleep detection

static unsigned long lastAccelSampleTime = 0;

static bool refreshAccelSampleIfStale(const unsigned long sampleInterval)
{
  if (!accelerometerEnabled || !g_accelerometer_initialized)
  {
    return false;
  }

  const unsigned long now = millis();
  if (!accelSampleValid || (now - lastAccelSampleTime) >= sampleInterval)
  {
    PROFILE_SECTION("AccelRead");
    accel.read();

    const float x = accel.x_g;
    const float y = accel.y_g;
    const float z = accel.z_g;

    // Calculate the movement delta from last reading
    lastAccelDeltaX = fabs(x - prevAccelX);
    lastAccelDeltaY = fabs(y - prevAccelY);
    lastAccelDeltaZ = fabs(z - prevAccelZ);

    // Store current values for next comparison and reuse across effects
    prevAccelX = x;
    prevAccelY = y;
    prevAccelZ = z;
    lastAccelSampleTime = now;
    accelSampleValid = true;

    // Update cached comparisons for both thresholds so other checks can reuse this sample
    lastMotionAboveShake = (lastAccelDeltaX > SHAKE_THRESHOLD) || (lastAccelDeltaY > SHAKE_THRESHOLD) || (lastAccelDeltaZ > SHAKE_THRESHOLD);
    lastMotionAboveSleep = (lastAccelDeltaX > SLEEP_THRESHOLD) || (lastAccelDeltaY > SLEEP_THRESHOLD) || (lastAccelDeltaZ > SLEEP_THRESHOLD);
  }

  return accelSampleValid;
}

bool getLatestAcceleration(float &x, float &y, float &z, const unsigned long sampleInterval)
{
  if (!refreshAccelSampleIfStale(sampleInterval))
  {
    return false;
  }

  x = prevAccelX;
  y = prevAccelY;
  z = prevAccelZ;
  return true;
}

bool detectMotion()
{
  if (!accelerometerEnabled || !g_accelerometer_initialized)
  {
    return false; // Guard against uninitialised accelerometer
  }

  const unsigned long sampleInterval = useShakeSensitivity ? MOTION_SAMPLE_INTERVAL_FAST : MOTION_SAMPLE_INTERVAL_SLOW;
  refreshAccelSampleIfStale(sampleInterval);

  if (currentView == VIEW_PIXEL_DUST)
  {
    useShakeSensitivity = false; // Disable shake-to-spiral while sand is active.
    return false;
  }

  // Return the cached comparison for the desired threshold without re-reading.
  return useShakeSensitivity ? lastMotionAboveShake : lastMotionAboveSleep;
}

void enterSleepMode()
{
  if (sleepModeActive)
    return; // Already sleeping
  Serial.println("Entering sleep mode");
  sleepModeActive = true;
  micSetEnabled(false);
  preSleepView = currentView;                   // Save current view
  dma_display->setBrightness8(sleepBrightness); // Lower display brightness
  updateGlobalBrightnessScale(sleepBrightness);

  reduceCPUSpeed(); // Reduce CPU speed for power saving

  sleepFrameInterval = 100; // 10 FPS during sleep to save power

  /// Reduce BLE activity
  if (!deviceConnected)
  {
    NimBLEDevice::getAdvertising()->stop();               // Stop normal advertising
    vTaskDelay(pdMS_TO_TICKS(10));                        // Short delay
    NimBLEDevice::getAdvertising()->setMinInterval(2400); // 1500 ms
    NimBLEDevice::getAdvertising()->setMaxInterval(4800); // 3000 ms
    NimBLEDevice::startAdvertising();
    Serial.println("Reduced BLE Adv interval for sleep.");
  }
  else
  {
    // Maybe send a sleep notification?
    if (pTemperatureCharacteristic != nullptr)
    { // Check if characteristic exists
      char sleepMsg[] = "Sleep";
      pTemperatureCharacteristic->setValue(sleepMsg);
      notifyBleTask();
      // pTemperatureCharacteristic->notify(); // Consider notifyPending
    }
  }
  // No need to change sleepFrameInterval here, main loop timing controls frame rate
}

void checkSleepMode()
{
  const unsigned long now = millis();
  // Ensure we use the correct sensitivity for wake-up/sleep checks
  useShakeSensitivity = false; // Use SLEEP_THRESHOLD when checking general activity/wake conditions
  bool motionDetectedByAccel = false;
  if (accelerometerEnabled && g_accelerometer_initialized)
  { // Only check accel if enabled and initialized
    const bool sampleFresh = accelSampleValid && (now - lastAccelSampleTime) < MOTION_SAMPLE_INTERVAL_SLOW;
    if (!sampleFresh)
    {
      detectMotion(); // Updates cached deltas for both thresholds
    }
    motionDetectedByAccel = lastMotionAboveSleep;
  }

  if (sleepModeActive)
  {
    // --- Currently Sleeping ---
    if (motionDetectedByAccel)
    {
      Serial.println("Motion detected while sleeping, waking up..."); // DEBUG
      wakeFromSleepMode();                                            // Call the wake function
                                                                      // wakeFromSleepMode already sets sleepModeActive = false and resets lastActivityTime
    }
    // No need to check timeout when already sleeping
  }
  else
  {
    // --- Currently Awake ---
    if (motionDetectedByAccel)
    {
      // Any motion resets the activity timer when awake
      lastActivityTime = now;
      // Serial.println("Motion detected while awake, activity timer reset."); // DEBUG Optional
    }
    else
    {
      // No motion detected while awake, check timeout for sleep entry
      // Ensure sleepModeEnabled config flag is checked
      const unsigned long inactivityMs = (now >= lastActivityTime) ? (now - lastActivityTime) : 0;
      if (sleepModeEnabled && (inactivityMs > SLEEP_TIMEOUT_MS))
      {
        Serial.println("Inactivity timeout reached, entering sleep..."); // DEBUG
        enterSleepMode();
      }
    }
  }
}

void loop()
{

  //Serial.println(apds.readProximity());
  
  // unsigned long frameStartTimeMillis = millis(); // Timestamp at frame start
  const unsigned long loopNow = millis();

  // Check BLE connection status (low frequency check is fine)
  // bool isConnected = NimBLEDevice::getServer()->getConnectedCount() > 0;
  {
    PROFILE_SECTION("BLEStatusLED");
    handleBLEStatusLED(); // Update status LED based on connection
  }

  // --- Handle Inputs and State Updates ---

  // Check for motion and handle sleep/wake state
  // checkSleepMode(); // Handles motion detection, wake, and sleep entry

  // Only process inputs/updates if NOT in sleep mode
  if (!sleepModeActive)
  {
    // --- Motion Detection (for shake effect to change view) ---
    {
      PROFILE_SECTION("MotionDetection");
      if (accelerometerEnabled && g_accelerometer_initialized && currentView != VIEW_FLUID_EFFECT && currentView != VIEW_DINO_GAME && !isEyeBouncing && !proximityLatchedHigh)
      {
        useShakeSensitivity = true; // Use high threshold for shake detection
        if (detectMotion())
        { // detectMotion uses the current useShakeSensitivity
          if (currentView != VIEW_SPIRAL_EYES)
          {                              // Prevent re-triggering if already spiral
            previousView = currentView;  // Save the current view.
            currentView = VIEW_SPIRAL_EYES;             // Switch to spiral eyes view
            spiralStartMillis = loopNow; // Record the trigger time.
            LOG_DEBUG_LN("Shake detected! Switching to Spiral View.");
            notifyBleTask();
            lastActivityTime = loopNow; // Shake is activity
          }
        }
        useShakeSensitivity = false; // Switch back to low threshold for general sleep/wake checks
      }
    }

// --- Handle button inputs for view changes ---
#if defined(ARDUINO_ADAFRUIT_MATRIXPORTAL_ESP32S3) || defined(BUTTONS_AVAILABLE) // Add define if you use external buttons
    // Hold both buttons to clear BLE bonds and restart pairing.
    static bool pairingHoldActive = false;
    static bool pairingHoldTriggered = false;
    static unsigned long pairingHoldStartMs = 0;
    static bool pairingHoldRaw = false;
    static bool pairingHoldStable = false;
    static unsigned long pairingHoldLastChangeMs = 0;

#if defined(BUTTON_UP) && defined(BUTTON_DOWN)
    const bool pairingHoldSample = (digitalRead(BUTTON_UP) == LOW) && (digitalRead(BUTTON_DOWN) == LOW);
#else
    const bool pairingHoldSample = false;
#endif

    if (pairingHoldSample != pairingHoldRaw)
    {
      pairingHoldRaw = pairingHoldSample;
      pairingHoldLastChangeMs = loopNow;
    }
    if ((loopNow - pairingHoldLastChangeMs) >= PAIRING_RESET_DEBOUNCE_MS)
    {
      pairingHoldStable = pairingHoldRaw;
    }

    const bool pairingHoldPressed = pairingHoldStable;

    if (pairingHoldPressed)
    {
      lastActivityTime = loopNow;
      if (!pairingHoldActive)
      {
        pairingHoldActive = true;
        pairingHoldStartMs = loopNow;
      }
      else if (!pairingHoldTriggered && (loopNow - pairingHoldStartMs >= PAIRING_RESET_HOLD_MS))
      {
        pairingHoldTriggered = true;
        resetBlePairing();
      }
    }
    else
    {
      pairingHoldActive = false;
      pairingHoldTriggered = false;
    }

    bool viewChangedByButton = false;

    if (!pairingHoldPressed && debounceButton(BUTTON_UP))
    {
      currentView = (currentView + 1);
      if (currentView >= totalViews)
        currentView = 0;
      viewChangedByButton = true;
      saveLastView(currentView);
      lastActivityTime = loopNow;
    }

    if (!pairingHoldPressed && debounceButton(BUTTON_DOWN))
    {
      PROFILE_SECTION("ButtonInputs");
      currentView = (currentView - 1);
      if (currentView < 0)
      {
        currentView = totalViews - 1;
      }
      viewChangedByButton = true;
      saveLastView(currentView);
      lastActivityTime = loopNow;
    }

    if (viewChangedByButton)
    {
      LOG_DEBUG("View changed by button: %d\n", currentView);
      notifyBleTask();
      // Reset specific view states if necessary when changing views
      if (currentView != VIEW_BLUSH_FACE)
      { // If leaving blush view
        blushState = BlushState::Inactive;
        blushBrightness = 0;
        // disableBlush(); // Clears pixels, but displayCurrentView will clear anyway
      }
      if (currentView != VIEW_SPIRAL_EYES)
      { // Reset spiral timer
        spiralStartMillis = 0;
      }
    }
#endif

    // --- Proximity Sensor Logic: Blush Trigger AND Eye Bounce Trigger ---
    // static unsigned long lastSensorReadTime = 0; // Already global
    runIfElapsed(loopNow, lastSensorReadTime, sensorInterval, [&]()
                 {
#if DEBUG_MODE
                   PROFILE_SECTION("ProximitySensor");
#endif
#if defined(APDS_AVAILABLE) // Ensure sensor is available
                  const unsigned long sensorNow = loopNow;

                   if (!shouldReadProximity(sensorNow))
                   {
#if DEBUG_PROXIMITY
                     LOG_PROX("Prox skip @%lu ms\n", sensorNow);
#endif
                     return; // Skip sensor work when nothing is pending
                   }

                   const uint32_t proxStartMicros = micros();
                   uint8_t proximity = apds.readProximity();
                   apds.clearInterrupt(); // Ensure latched interrupt doesn't block future samples
                   const uint32_t proxDuration = micros() - proxStartMicros;

#if DEBUG_MODE
                   if (proxDuration > SLOW_SECTION_THRESHOLD_US)
                   {
                     LOG_DEBUG("Slow proximity read: %lu us\n", static_cast<unsigned long>(proxDuration));
                   }
#endif
#if DEBUG_PROXIMITY
                   LOG_PROX("Prox=%u dt=%lu us\n", proximity, proxDuration);
#endif

                   bool bounceJustTriggered = false; // Flag to avoid double blush trigger

                   if (!proximityBaselineValid)
                   {
                     proximityBaseline = static_cast<float>(proximity);
                     proximityBaselineValid = true;
                   }

                   int baselineRounded = static_cast<int>(proximityBaseline + 0.5f);
                   uint8_t triggerThreshold = static_cast<uint8_t>(
                       constrain(baselineRounded + PROX_TRIGGER_DELTA, 0, 255));
                   uint8_t releaseThreshold = static_cast<uint8_t>(
                       constrain(baselineRounded + PROX_RELEASE_DELTA, 0, 255));

                   if (!proximityLatchedHigh && proximity <= releaseThreshold)
                   {
                     proximityBaseline += PROX_BASELINE_ALPHA * (static_cast<float>(proximity) - proximityBaseline);
                     baselineRounded = static_cast<int>(proximityBaseline + 0.5f);
                     triggerThreshold = static_cast<uint8_t>(
                         constrain(baselineRounded + PROX_TRIGGER_DELTA, 0, 255));
                     releaseThreshold = static_cast<uint8_t>(
                         constrain(baselineRounded + PROX_RELEASE_DELTA, 0, 255));
                   }

                   // Require the reading to rise above the trigger threshold and then fall below the release threshold before re-triggering
                   if (proximityLatchedHigh)
                   {
                     const bool timeout = (sensorNow - proximityLatchedAt) >= PROX_LATCH_TIMEOUT_MS;
                     if (proximity <= releaseThreshold || (timeout && proximity < triggerThreshold))
                     {
                       proximityLatchedHigh = false;
#if DEBUG_PROXIMITY
                       LOG_PROX("Prox released at %u (rel=%u base=%d)\n", proximity, releaseThreshold, baselineRounded);
#endif
                     }
                     else
                     {
#if DEBUG_PROXIMITY
                       LOG_PROX("Prox latched high (%u), skipping trigger\n", proximity);
#endif
                       return;
                     }
                   }

                   if (proximity < triggerThreshold)
                   {
                     return;
                   }

                   proximityLatchedHigh = true;
                   proximityLatchedAt = sensorNow;

                   if (currentView == VIEW_DINO_GAME)
                   {
                     queueDinoJump();
                     lastActivityTime = sensorNow;
                     return;
                   }

                   // Eye Bounce Trigger - Switch to View 17 (Circle Eyes) (MODIFIED)
                   if (!isEyeBouncing)
                   {
                     LOG_DEBUG_LN("Proximity! Starting eye bounce sequence & switching to Circle Eyes (View 17).");
#if DEBUG_PROXIMITY
                     LOG_PROX("Bounce trigger prox=%u view=%d\n", proximity, currentView);
#endif

                     // Store current view and switch to Circle Eyes (view 17)
                     if (currentView != VIEW_CIRCLE_EYES)
                     {
                       viewBeforeEyeBounce = currentView;
                       currentView = VIEW_CIRCLE_EYES; // Switch to "Circle Eyes" view
                       // saveLastView(currentView); // Optional: save temporary view 17
                       notifyBleTask();
                     }

                     isEyeBouncing = true;
                     eyeBounceStartTime = sensorNow;
                     eyeBounceCount = 0;
                     lastActivityTime = sensorNow;
                     bounceJustTriggered = true; // Mark that bounce (and view switch) happened

                     // Also trigger blush effect to happen ON view 17 (Circle Eyes)
                     if (blushState == BlushState::Inactive)
                     { // Only trigger if not already blushing
                       LOG_DEBUG_LN("Proximity! Also triggering blush effect on Circle Eyes.");
#if DEBUG_PROXIMITY
                       LOG_PROX("Blush-on-bounce prox=%u\n", proximity);
#endif
                       blushState = BlushState::FadeIn;
                       blushStateStartTime = sensorNow;
                       wasBlushOverlay = false;                       // Blush on view 17 is part of that view's temporary effect
                       originalViewBeforeBlush = viewBeforeEyeBounce; // Blush on view 17 uses context of viewBeforeEyeBounce
                     }
                   }

                   if (blushState == BlushState::Inactive && !bounceJustTriggered)
                   {
                     // Trigger blush if:
                     // 1. Proximity detected
                     // 2. Not already blushing
                     // 3. Bounce wasn't *just* triggered (which might have handled its own blush)
                     // 4. Current view is NOT the dedicated blush view (5)
                     //    AND current view is NOT the temporary bounce/circle view (17)
                     if (currentView != VIEW_BLUSH_FACE && currentView != VIEW_CIRCLE_EYES)
                     {
                       LOG_DEBUG_LN("Proximity! Triggering blush overlay effect.");
#if DEBUG_PROXIMITY
                       LOG_PROX("Blush overlay prox=%u view=%d\n", proximity, currentView);
#endif
                       blushState = BlushState::FadeIn;
                       blushStateStartTime = sensorNow;
                       lastActivityTime = sensorNow;

                       // This is a blush overlay on the current stable view
                       originalViewBeforeBlush = currentView; // Store the view that is getting the overlay
                       wasBlushOverlay = true;                // Mark that this blush is an overlay
                     }
                   }
#endif
                 });

    // --- Update Adaptive Brightness ---
    if (autoBrightnessEnabled)
    {
      runIfElapsed(loopNow, lastAutoBrightnessUpdate, AUTO_BRIGHTNESS_INTERVAL_MS, [&]()
                   {
                     PROFILE_SECTION("AutoBrightness");
                     maybeUpdateBrightness();
                   });
    }
    if (deviceConnected)
    {
      runIfElapsed(loopNow, lastLuxUpdateTime, LUX_UPDATE_INTERVAL_MS, [&]()
                   {
                     PROFILE_SECTION("LuxCharacteristic");
                     // Manual brightness is applied immediately on BLE write if autoBrightness is off.
                     updateLux(); // Update lux values
                   });
    }

    // --- Update Animation States ---
    {
      PROFILE_SECTION("AnimationUpdates");
      updateBlinkAnimation();     // Update blink animation once per loop
      updateEyeBounceAnimation(); // NEW: Update eye bounce animation progress
      updateIdleHoverAnimation(); // NEW: Update idle eye hover animation progress
    }

    if (blushState != BlushState::Inactive)
    {
      PROFILE_SECTION("BlushUpdate");
      updateBlush();
    }

    // --- Revert from Spiral View Timer ---
    // Use a local copy to avoid updating spiralStartMillis inside hasElapsedSince
    unsigned long spiralStartMillisCopy = spiralStartMillis;
    if (currentView == VIEW_SPIRAL_EYES && spiralStartMillisCopy > 0 && hasElapsedSince(loopNow, spiralStartMillisCopy, 5000))
    {
      LOG_DEBUG_LN("Spiral timeout, reverting view.");
      currentView = previousView;
      spiralStartMillis = 0;
      notifyBleTask();
    }

    // --- Update Temperature Sensor Periodically ---
    static unsigned long lastTempUpdateLocal = 0;       // Renamed to avoid conflict
    const unsigned long tempUpdateIntervalLocal = 5000; // 5 seconds
    runIfElapsed(loopNow, lastTempUpdateLocal, tempUpdateIntervalLocal, [&]()
                 {
#if DEBUG_MODE
                   PROFILE_SECTION("TemperatureUpdate");
#endif
                   maybeUpdateTemperature();
                 });

  } // End of (!sleepModeActive) block

  // --- Display Rendering ---
  {
    PROFILE_SECTION("MouthMovement");
  }
  // displayCurrentView(currentView); // Draw the appropriate view based on state

  // --- Check sleep again AFTER processing inputs ---
  // This allows inputs like button presses or BLE commands to reset the activity timer *before* checking for sleep timeout
  runIfElapsed(loopNow, lastSleepCheckTime, SLEEP_CHECK_INTERVAL_MS, [&]()
               {
                 PROFILE_SECTION("SleepModeCheck");
                 checkSleepMode();
               });

  // Check for changes in brightness
  // checkBrightness();

  // --- Frame Rate Calculation ---
  {
    PROFILE_SECTION("FPSCalc");
    calculateFPS(); // Update FPS counter
  }

  taskYIELD(); // Give other tasks (BLE notifications, etc.) time on this core

  // vTaskDelay(pdMS_TO_TICKS(5)); // yield to the display & BLE tasks

  /*
  // If the current view is one of the plasma views, increase the interval to reduce load
  if (currentView == VIEW_PATTERN_PLASMA || currentView == VIEW_PLASMA_FACE) {
    baseInterval = 10; // Use 15 ms for plasma view
  }
  */
}
