#ifdef IDF_BUILD
#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/gpio.h>
#include "sdkconfig.h"
#endif

#include <Arduino.h>
#include <Adafruit_GFX.h>
 #include <Wire.h>                             // For I2C sensors
#ifdef VIRTUAL_PANE
#include <ESP32-VirtualMatrixPanel-I2S-DMA.h>
#else
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#endif

#include <Adafruit_PixelDust.h>   // For sand simulation
#include "main.h"
#include "customEffects/flameEffect.h"
#include "customEffects/fluidEffect.h"
//#include "customEffects/pixelDustEffect.h" // New effect

// BLE Libraries
#include <NimBLEDevice.h>

#include <vector> //For chunking example
#include <cstring>

//#define PIXEL_COLOR_DEPTH_BITS 16 // 16 bits per pixel

// #include "EffectsLayer.hpp" // FastLED CRGB Pixel Buffer for which the patterns are drawn
// EffectsLayer effects(VPANEL_W, VPANEL_H);

// Setup functions for adaptive brightness using APDS9960:
// Call setupAdaptiveBrightness() in your setup() to initialize the sensor.

// ----------------------------------------------------------------
// ----------------------------------------------------------------
// ------------------- LumiFur Global Variables -------------------
// ----------------------------------------------------------------
// ----------------------------------------------------------------
FluidEffect* fluidEffectInstance = nullptr; // Global pointer for our fluid effect object
// FluidEffect* fluidEffectInstance = nullptr; // Keep or remove if replacing
// PixelDustEffect* pixelDustEffectInstance = nullptr; // Add this
// Brightness control
//  Retrieve the brightness value from preferences
bool g_accelerometer_initialized = false;

// --- Performance Tuning ---
// Target ~50-60 FPS. Adjust as needed based on view complexity.
const unsigned long targetFrameIntervalMillis = 10; // ~100 FPS pacing
// ---

bool brightnessChanged = false;

// View switching
// View switching
uint8_t currentView = VIEW_FLAME_EFFECT;   // Current & initial view being displayed
const int totalViews = TOTAL_VIEWS; // Total number of views is now calculated automatically
// int userBrightness = 20; // Default brightness level (0-255)


// Plasma animation optimization
uint16_t plasmaFrameDelay = 15; // ms between plasma updates (was 10)
// Spiral animation optimization
unsigned long rotationFrameInterval = 15; // ms between spiral rotation updates (was 11)


unsigned long spiralStartMillis = 0; // Global variable to record when spiral view started
int previousView = 0;              // Global variable to store the view before spiral

// Maw switching
int currentMaw = 1;              // Current & initial maw being displayed
const int totalMaws = 2;         // Total number of maws to cycle through
unsigned long mawChangeTime = 0; // Time when maw was changed
bool mawTemporaryChange = false; // Whether the maw is temporarily changed
bool mouthOpen = false;         // Whether the mouth is open
static unsigned long lastMouthTriggerTime = 0;
const unsigned long mouthOpenHoldTime = 300; // ms to hold open
// Loading Bar
int loadingProgress = 0; // Current loading progress (0-100)
int loadingMax = 100;    // Maximum loading value

// Blinking effect variables
unsigned long lastEyeBlinkTime = 0;  // Last time the eyes blinked
unsigned long nextBlinkDelay = 1000; // Random delay between blinks
int blinkProgress = 0;               // Progress of the blink (0-100%)
bool isBlinking = false;             // Whether a blink is in progress
bool manualBlinkTrigger = false;     // To trigger a blink manually

// Blink animation control with initialization
struct blinkState
{
  unsigned long startTime = 0;
  unsigned long durationMs = 700; // Default duration
  unsigned long closeDuration = 50;
  unsigned long holdDuration = 20;
  unsigned long openDuration = 50;
} blinkState;

int blinkDuration = 700;          // Initial time for a full blink (milliseconds)
const int minBlinkDuration = 300; // Minimum time for a full blink (ms)
const int maxBlinkDuration = 800; // Maximum time for a full blink (ms)
const int minBlinkDelay = 250;   // Minimum time between blinks (ms)
const int maxBlinkDelay = 5000;   // Maximum time between blinks (ms)
constexpr uint16_t SCROLL_TEXT_INTERVAL_MS = 36;
constexpr uint16_t SCROLL_BACKGROUND_INTERVAL_MS = 45;
constexpr int16_t SCROLL_TEXT_GAP = 12;
static unsigned long lastScrollTick = 0;
static unsigned long lastBackgroundTick = 0;
static uint8_t scrollingBackgroundOffset = 0;
static uint8_t scrollingColorOffset = 0;
static bool scrollingTextInitialized = false;


float globalBrightnessScale = 0.0f;
uint16_t globalBrightnessScaleFixed = 256;
bool facePlasmaDirty = true;

inline void updateGlobalBrightnessScale(uint8_t brightness) {
    globalBrightnessScale = brightness / 255.0f;
    globalBrightnessScaleFixed = static_cast<uint16_t>((static_cast<uint32_t>(brightness) * 256u + 127u) / 255u);
    facePlasmaDirty = true;
}

inline void setBlinkProgress(int newValue) {
    if (blinkProgress != newValue) {
        facePlasmaDirty = true;
    }
    blinkProgress = newValue;
}

// Eye bounce effect variables
bool isEyeBouncing = false;
unsigned long eyeBounceStartTime = 0;
const unsigned long EYE_BOUNCE_DURATION = 400; // Duration of one bounce cycle (ms)
const int EYE_BOUNCE_AMPLITUDE = 5;         // How many pixels down the eyes will move
int currentEyeYOffset = 0;                  // Vertical offset from proximity bounce
int eyeBounceCount = 0;                     // Counter for the number of bounces
const int MAX_EYE_BOUNCES = 10;              // Desired number of bounces                  // Vertical offset from idle hover
static uint8_t viewBeforeEyeBounce = 0; // <<< NEW: Dedicated variable

// Idle Eye Hover Variables (MODIFIED)
const int IDLE_HOVER_AMPLITUDE_Y = 1;       // Max 1 pixel up/down for idle hover
const int IDLE_HOVER_AMPLITUDE_X = 1;       // Max 1 pixel left/right for idle hover (NEW)
const unsigned long IDLE_HOVER_PERIOD_MS_Y = 3000; // 3 seconds for one full Y hover cycle
const unsigned long IDLE_HOVER_PERIOD_MS_X = 4200; // ~4.2 seconds for one full X hover cycle (NEW - slightly different period for more organic movement)
int idleEyeYOffset = 0;                     // Vertical offset from idle hover
int idleEyeXOffset = 0;                     // Horizontal offset from idle hover (NEW)

// Global constants for each sensitivity level
const float SLEEP_THRESHOLD = 1.0;  // for sleep mode detection
const float SHAKE_THRESHOLD = 1.0; // for shake detection
// Global flag to control which threshold to use
bool useShakeSensitivity = true;

// Define blush state using an enum
enum BlushState {
  BLUSH_INACTIVE,
  BLUSH_FADE_IN,
  BLUSH_FULL,
  BLUSH_FADE_OUT
};
BlushState blushState = BLUSH_INACTIVE;

// Blush effect variables
unsigned long blushStateStartTime = 0;
const unsigned long fadeInDuration = 2000; // Duration for fade-in (2 seconds)
const unsigned long fullDuration = 6000;   // Full brightness time after fade-in (6 seconds)
// Total time from trigger to start fade-out is fadeInDuration + fullDuration = 8 seconds.
const unsigned long fadeOutDuration = 2000; // Duration for fade-out (2 seconds)
bool isBlushFadingIn = false;               // Flag to indicate we are in the fade‑in phase
static uint8_t originalViewBeforeBlush = 0;
static bool wasBlushOverlay = false;

// Blush brightness variable (0-255)
uint8_t blushBrightness = 0;

// Non-blocking sensor read interval
unsigned long lastSensorReadTime = 0;
const unsigned long sensorInterval = 150; // sensor read every 50 ms

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
uint8_t preSleepView = 4;                  // Store the view before sleep
uint8_t sleepBrightness = 15;              // Brightness level during sleep (0-255)
uint8_t normalBrightness = userBrightness; // Normal brightness level
volatile bool notifyPending = false;
unsigned long sleepFrameInterval = 50; // Frame interval in ms (will be changed during sleep)


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

const float FULL_SPIRAL_ROTATION_SPEED_RAD_PER_UPDATE = 0.5f;
const uint8_t FULL_SPIRAL_COLOR_SCROLL_SPEED = 2.5;

// Logarithmic Spiral Coefficients (NEW/MODIFIED)
const float LOG_SPIRAL_A_COEFF = 2.0f;   // Initial radius factor (when theta is small or exp(0))
                                         // Smaller 'a' means it starts smaller/tighter from center.
const float LOG_SPIRAL_B_COEFF = 0.08f;  // Controls how tightly wound the spiral is.
                                         // Smaller 'b' makes it wind more slowly (appearing tighter initially for a given theta range).
                                         // Larger 'b' makes it expand much faster.
                                         // Positive 'b' for outward spiral, negative for inward.

const float SPIRAL_ARM_COLOR_FACTOR = 5.0f; // Could be based on 'r' now instead of 'theta_arm'
const float SPIRAL_THICKNESS_RADIUS = 1.0f;    // Start with 0 for performance, then increase

static TaskHandle_t bleNotifyTaskHandle = NULL;

void calculateFPS() {
    frameCounter++;
    unsigned long now = millis();
    if (now - lastFpsCalcTime >= 1000) {
        currentFPS = frameCounter;
        frameCounter = 0;
        lastFpsCalcTime = now;
        // Serial.printf("FPS: %d\n", currentFPS); // Optional: Print FPS once per second
    }
}

void maybeUpdateTemperature() {
  unsigned long lastTempUpdate = 0;
const unsigned long tempUpdateInterval = 5000; // 5 seconds
  if (deviceConnected && (millis() - lastTempUpdate >= tempUpdateInterval)) {
    updateTemperature();
    lastTempUpdate = millis();
  }
}

void displayCurrentView(int view);

void wakeFromSleepMode() {
  if (!sleepModeActive)
    return; // Already awake

  Serial.println(">>> Waking from sleep mode <<<");
  sleepModeActive = false;
  currentView = preSleepView; // Restore previous view

  // Restore normal CPU speed
  restoreNormalCPUSpeed();

  // Restore normal frame rate
  sleepFrameInterval = 5; // Back to ~90 FPS

  // Restore normal brightness
  dma_display->setBrightness8(userBrightness);
  updateGlobalBrightnessScale(userBrightness);

  lastActivityTime = millis(); // Reset activity timer

  // Notify all clients if connected
  if (deviceConnected) {
    // Also send current view back to the app
    uint8_t bleViewValue = static_cast<uint8_t>(currentView);
    pFaceCharacteristic->setValue(&bleViewValue, 1);
    pFaceCharacteristic->notify();

    // Send a temperature update
    maybeUpdateTemperature();
  }

  // Restore normal BLE advertising if not connected
  if (!deviceConnected) {
    NimBLEDevice::getAdvertising()->setMinInterval(160); // 100 ms (default)
    NimBLEDevice::getAdvertising()->setMaxInterval(240); // 150 ms (default)
    NimBLEDevice::startAdvertising();
  }
}

// Callback class for handling writes to the command characteristic
class CommandCallbacks : public NimBLECharacteristicCallbacks {

  // Override the onWrite method
  void onWrite(NimBLECharacteristic* pCharacteristic) {
      // Get the value written by the client
      std::string rxValue = pCharacteristic->getValue();

      if (rxValue.length() > 0) {
          uint8_t commandCode = rxValue[0]; // Assuming single-byte commands

          Serial.print("Command Characteristic received write, command code: 0x");
          Serial.println(commandCode, HEX);

          // Process the command
          switch (commandCode) {
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
      } else {
          Serial.println("Command Characteristic received empty write.");
      }
  }
} cmdCallbacks;

// New Callback class for handling notifications related to Temperature Logs
class TemperatureLogsCallbacks : public NimBLECharacteristicCallbacks {
  public:
      // Optionally, implement onSubscribe to log when a client subscribes to notifications
      void onSubscribe(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &connInfo, uint16_t subValue) override {
          std::string str = "Client subscribed to Temperature Logs notifications, Client ID: ";
          str += connInfo.getConnHandle();
          str += " Address: ";
          str += connInfo.getAddress().toString();
          Serial.printf("%s\n", str.c_str());
      } 
      
    }; TemperatureLogsCallbacks logsCallbacks;

// Class to handle characteristic callbacks
class CharacteristicCallbacks : public NimBLECharacteristicCallbacks {
  void onRead(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &connInfo) override
  {
    uint8_t bleViewValue = static_cast<uint8_t>(currentView);
    pCharacteristic->setValue(&bleViewValue, 1);
    Serial.printf("Read request - returned view: %d\n", bleViewValue);
  }

  void onWrite(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &connInfo) {
    std::string value = pCharacteristic->getValue();
    if (value.length() < 1)
      return;

    uint8_t newView = value[0];

    // --- Handle Wake-up First ---
    if (sleepModeActive) {
      Serial.println("BLE write received while sleeping, waking up..."); // DEBUG
      wakeFromSleepMode();
      // NOTE: wakeFromSleepMode resets lastActivityTime and sets sleepModeActive = false
      // Now that we are awake, proceed to process the command below.
  }
    // Normal view change
    if (newView >= 0 && newView < totalViews && newView != currentView)
    {
      currentView = newView;
      saveLastView(currentView);               // Persist the new view
      lastActivityTime = millis(); // BLE command counts as activity
      Serial.printf("Write request - new view: %d\n", currentView);
      // pCharacteristic->notify();
      //notifyPending = true;
      xTaskNotifyGive(bleNotifyTaskHandle);
    } else if (newView >= totalViews) {
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
class ConfigCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo &connInfo) override {
    std::string value = pCharacteristic->getValue();
    Serial.println("Received configuration update:");
    // We expect exactly 4 bytes: one for each Boolean setting.
    if (value.length() == 4) {
    bool oldAutoBrightnessEnabled = autoBrightnessEnabled; // Store old state
      // Decode each setting; non-zero is true, zero is false.
      autoBrightnessEnabled = (value[0] != 0);
      accelerometerEnabled  = (value[1] != 0);
      sleepModeEnabled      = (value[2] != 0);
      auroraModeEnabled     = (value[3] != 0);
      // Log the updated settings.
      Serial.print("  Auto Brightness: ");
      Serial.println(autoBrightnessEnabled ? "Enabled" : "Disabled");
      Serial.print("  Accelerometer:   ");
      Serial.println(accelerometerEnabled ? "Enabled" : "Disabled");
      Serial.print("  Sleep Mode:      ");
      Serial.println(sleepModeEnabled ? "Enabled" : "Disabled");
      Serial.print("  Aurora Mode:     ");
      Serial.println(auroraModeEnabled ? "Enabled" : "Disabled");
      // Apply configuration changes based on the new settings.
      applyConfigOptions();
      if (oldAutoBrightnessEnabled != autoBrightnessEnabled)
      { // If the setting actually changed
        if (autoBrightnessEnabled)
        {
          Serial.println("Auto brightness has been ENABLED. Adaptive logic will take over.");
        } else {
             Serial.println("Auto brightness has been DISABLED. Applying user-set brightness.");
             dma_display->setBrightness8(userBrightness);
             updateGlobalBrightnessScale(userBrightness);
             Serial.printf("Applied manual brightness: %u\n", userBrightness);
         }
      }

    } else {
      Serial.println("Error: Config payload is not 4 bytes.");
    }
  }
};
// Reuse one instance to avoid heap fragmentation
static ConfigCallbacks configCallbacks;


class BrightnessCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* pChr, NimBLEConnInfo &connInfo) override {
    auto val = pChr->getValue();
    if (val.size() >= 1) {
      userBrightness = (uint8_t)val[0]; // Store the app-set brightness
      brightnessChanged = true;  // Flag that brightness was set by user
      // ADDED: Apply brightness immediately if auto-brightness is off
      if (!autoBrightnessEnabled) {
        dma_display->setBrightness8(userBrightness);
        updateGlobalBrightnessScale(userBrightness);
        Serial.printf("Applied manual brightness: %u\n", userBrightness);
      }
      Serial.printf("Brightness set to %u\n", userBrightness);
    }
  }
};
static BrightnessCallbacks brightnessCallbacks;

class DescriptorCallbacks : public NimBLEDescriptorCallbacks {
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

void handleBLEStatusLED() {
  if (deviceConnected != oldDeviceConnected) {
    if (deviceConnected) {
      statusPixel.setPixelColor(0, 0, 100, 0); // Green when connected
    } else {
      statusPixel.setPixelColor(0, 0, 0, 0); // Off when disconnected
      NimBLEDevice::startAdvertising();
    }
    //statusPixel.show();
    oldDeviceConnected = deviceConnected;
  }
  if (!deviceConnected){
    fadeInAndOutLED(0, 0, 100); // Blue fade when disconnected
  }
  statusPixel.show();
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

uint16_t colorWheel(uint8_t pos) {
  if (pos < 85) {
    return dma_display->color565(pos * 3, 255 - pos * 3, 0);
  } else if (pos < 170) {
    pos -= 85;
    return dma_display->color565(255 - pos * 3, 0, pos * 3);
  }
  pos -= 170;
  return dma_display->color565(0, pos * 3, 255 - pos * 3);
}

static void initializeScrollingText() {
  if (!dma_display) {
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

  scrollingBackgroundOffset = 0;
  scrollingColorOffset = 0;
  lastScrollTick = millis();
  lastBackgroundTick = lastScrollTick;
  scrollingTextInitialized = true;
}

static void drawScrollingBackground(uint8_t offset) {
  if (!dma_display) {
    return;
  }

  const int width = dma_display->width();
  const int height = dma_display->height();

  for (int y = 0; y < height; ++y) {
    const uint8_t paletteIndex = sin8(static_cast<uint8_t>(y * 8) + offset);
    const CRGB color = ColorFromPalette(CloudColors_p, paletteIndex);
    dma_display->drawFastHLine(0, y, width, dma_display->color565(color.r, color.g, color.b));
  }
}

static void renderScrollingTextView() {
  if (!scrollingTextInitialized) {
    initializeScrollingText();
  }

  const unsigned long now = millis();

  if (now - lastBackgroundTick >= SCROLL_BACKGROUND_INTERVAL_MS) {
    lastBackgroundTick = now;
    ++scrollingBackgroundOffset;
  }

  drawScrollingBackground(scrollingBackgroundOffset);

  if (now - lastScrollTick >= SCROLL_TEXT_INTERVAL_MS) {
    lastScrollTick = now;
    --textX;
    if (textX <= textMin) {
      textX = dma_display->width() + SCROLL_TEXT_GAP;
    }
    scrollingColorOffset = static_cast<uint8_t>(scrollingColorOffset + 3);
  }

  drawText(scrollingColorOffset);
}

uint16_t plasmaSpeed = 2; // Lower = slower animation

void drawPlasmaXbm(int x, int y, int width, int height, const char *xbm,
                   uint8_t time_offset = 0, float scale = 5.0f, float animSpeed = 0.2f)
{
    const int byteWidth = (width + 7) >> 3;
    if (byteWidth <= 0) {
        return;
    }

    const uint16_t scaleFixed = static_cast<uint16_t>(scale * 256.0f);
    if (scaleFixed == 0) {
        return;
    }
    const uint16_t scaleHalfFixed = scaleFixed >> 1;
    int32_t startXFixed = static_cast<int32_t>(x) * static_cast<int32_t>(scaleFixed);

    const uint16_t animSpeedFixed = static_cast<uint16_t>(animSpeed * 256.0f);
    const uint32_t effectiveTimeFixed = static_cast<uint32_t>(time_counter) * animSpeedFixed;
    const uint8_t t = static_cast<uint8_t>(effectiveTimeFixed >> 8);
    const uint8_t t2 = static_cast<uint8_t>(t >> 1);
    const uint8_t t3 = static_cast<uint8_t>(((effectiveTimeFixed / 3U) >> 8) & 0xFF);

    const uint16_t brightnessScale = globalBrightnessScaleFixed;

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
                const uint8_t sin_val = sin8(static_cast<uint8_t>((xFixed >> 8) + t));
                const uint8_t sin_val2 = sin8(static_cast<uint8_t>((tempFixed >> 8) + t3));
                const uint8_t v = sin_val + cos_val + sin_val2;

                const uint8_t paletteIndex = static_cast<uint8_t>(v + time_offset);
                CRGB color = ColorFromPalette(currentPalette, paletteIndex);
                const uint16_t scale = brightnessScale;
                color.r = static_cast<uint8_t>((static_cast<uint16_t>(color.r) * scale + 128) >> 8);
                color.g = static_cast<uint8_t>((static_cast<uint16_t>(color.g) * scale + 128) >> 8);
                color.b = static_cast<uint8_t>((static_cast<uint16_t>(color.b) * scale + 128) >> 8);

                dma_display->drawPixel(x + i, yj, dma_display->color565(color.r, color.g, color.b));
            }

            xFixed += scaleFixed;
            tempFixed += scaleHalfFixed;
        }
    }
}

void drawText(int colorWheelOffset) {
    // Update text position
    textX--;

    // Check if text has scrolled off screen and reset
    if (textX < textMin) {
        textX = dma_display->width();
    }

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
void drawBitmapWithBlink(int x, int y, int width, int height, const uint8_t *bitmap, uint16_t color, int progress) {
    int byteWidth = (width + 7) / 8;
    float center_y = (height - 1) / 2.0f;
    
    // This formula replicates the "w" calculation from the emulator for the squash effect
    float w = 0.005f + (1.0f - 0.005f) * (progress / 100.0f);

    for (int j = 0; j < height; j++) {
        for (int i = 0; i < width; i++) {
            if (pgm_read_byte(&bitmap[j * byteWidth + i / 8]) & (0x80 >> (i % 8))) {
                // Calculate brightness based on vertical distance from center, modulated by 'w'
                float blinkBrightness = pow(2, -w * pow(j - center_y, 2));
                if (blinkBrightness < 0.01) continue;

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
                        uint8_t time_offset = 0, float scale = 5.0, float animSpeed = 0.2f) {
    int byteWidth = (width + 7) / 8;
    float center_y = (height - 1) / 2.0f;
    
    // --- Blink Effect Calculation ---
    float w = 0.005f + (1.0f - 0.005f) * (progress / 100.0f);

    // --- Plasma Effect Setup (if enabled) ---
    uint8_t t = 0, t2 = 0, t3 = 0;
    float scaleHalf = 0;
    if (usePlasma) {
        float effectiveTimeFloat = time_counter * animSpeed;
        t = (uint8_t)effectiveTimeFloat;
        t2 = (uint8_t)(effectiveTimeFloat / 2);
        t3 = (uint8_t)(effectiveTimeFloat / 3);
        scaleHalf = scale * 0.5f;
    }

    for (int j = 0; j < height; j++) {
        // --- OPTIMIZATION: Calculate blink brightness once per row ---
        float blinkBrightness = powf(2.0f, -w * powf(j - center_y, 2));
        // If the entire row is too dim to be visible, skip it completely.
        if (blinkBrightness < 0.01f) continue;

        // Pre-calculate plasma values that are constant for the row
        float y_val_plasma = (y + j) * scale;
        float tempSum_plasma = (x + y + j) * scaleHalf;
        float x_val_plasma = x * scale;
        uint8_t cos_val_plasma = 0;
        if (usePlasma) {
            cos_val_plasma = cos8(y_val_plasma + t2);
        }

        for (int i = 0; i < width; i++) {
            // Check if the pixel in the bitmap is set
            if (pgm_read_byte(&bitmap[j * byteWidth + i / 8]) & (0x80 >> (i % 8))) {
                
                CRGB final_color;

                if (usePlasma) {
                    // --- Plasma Color Calculation ---
                    uint8_t sin_val = sin8(x_val_plasma + t);
                    uint8_t sin_val2 = sin8(tempSum_plasma + i * scaleHalf + t3);
                    uint8_t v = sin_val + cos_val_plasma + sin_val2;
                    final_color = ColorFromPalette(currentPalette, v + time_offset);
                } else {
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
            if (usePlasma) {
                x_val_plasma += scale;
            }
        }
    }
}

// NEW: Update idle eye hover animation
void updateIdleHoverAnimation() {
    const uint32_t currentMs = millis();

    const int prevIdleYOffset = idleEyeYOffset;
    const int prevIdleXOffset = idleEyeXOffset;

    if (IDLE_HOVER_PERIOD_MS_Y > 0) {
        const uint32_t phaseY = (static_cast<uint64_t>(currentMs % IDLE_HOVER_PERIOD_MS_Y) << 16) / IDLE_HOVER_PERIOD_MS_Y;
        const int32_t sinY = sin16(static_cast<uint16_t>(phaseY));
        idleEyeYOffset = static_cast<int>((sinY * IDLE_HOVER_AMPLITUDE_Y + 16384) >> 15);
    } else {
        idleEyeYOffset = 0;
    }

    if (IDLE_HOVER_PERIOD_MS_X > 0) {
        const uint32_t phaseX = (static_cast<uint64_t>(currentMs % IDLE_HOVER_PERIOD_MS_X) << 16) / IDLE_HOVER_PERIOD_MS_X;
        const int32_t cosX = cos16(static_cast<uint16_t>(phaseX));
        idleEyeXOffset = static_cast<int>((cosX * IDLE_HOVER_AMPLITUDE_X + 16384) >> 15);
    } else {
        idleEyeXOffset = 0;
    }

    if (idleEyeYOffset != prevIdleYOffset || idleEyeXOffset != prevIdleXOffset) {
        facePlasmaDirty = true;
    }
}



// NEW: Update eye bounce animation (Modified for multiple bounces)
void updateEyeBounceAnimation() {
    int newOffset = currentEyeYOffset;

    if (!isEyeBouncing) {
        eyeBounceCount = 0;
        if (newOffset != 0) {
            newOffset = 0;
            facePlasmaDirty = true;
        }
        currentEyeYOffset = newOffset;
        return;
    }

    unsigned long elapsed = millis() - eyeBounceStartTime;

    if (elapsed >= EYE_BOUNCE_DURATION) {
        eyeBounceCount++;
        if (eyeBounceCount >= MAX_EYE_BOUNCES) {
            isEyeBouncing = false;
            newOffset = 0;
            eyeBounceCount = 0;

            if (currentView == 17) {
                currentView = viewBeforeEyeBounce;
                saveLastView(currentView);
                Serial.printf("Eye bounce finished. Reverting to view: %d\n", currentView);
                if (deviceConnected) {
                    xTaskNotifyGive(bleNotifyTaskHandle);
                }
            }

            if (currentEyeYOffset != newOffset) {
                facePlasmaDirty = true;
            }
            currentEyeYOffset = newOffset;
            return;
        } else {
            eyeBounceStartTime = millis();
            elapsed = 0;
        }
    }

    const unsigned long halfDuration = EYE_BOUNCE_DURATION / 2;
    if (elapsed < halfDuration) {
        newOffset = map(elapsed, 0, halfDuration, 0, EYE_BOUNCE_AMPLITUDE);
    } else {
        newOffset = map(elapsed, halfDuration, EYE_BOUNCE_DURATION, EYE_BOUNCE_AMPLITUDE, 0);
    }

    if (newOffset != currentEyeYOffset) {
        currentEyeYOffset = newOffset;
        facePlasmaDirty = true;
    }
}

void drawPlasmaFace() {
   // Combine bounce and idle hover offsets
  // Combine bounce and idle hover offsets
  int final_y_offset = currentEyeYOffset + idleEyeYOffset;
  int final_x_offset_right = idleEyeXOffset; // X offset for the right eye (0,0 based)
  int final_x_offset_left = idleEyeXOffset;  // X offset for the left eye (96,0 based)

  // Draw eyes with different plasma parameters
  int y_offset = currentEyeYOffset; // Get current bounce offset for eyes
  drawPlasmaXbm(0 + final_x_offset_right, 0 + final_y_offset, 32, 16, Eye, 0, 1.0);     // Right eye
  drawPlasmaXbm(96 + final_x_offset_left, 0 + final_y_offset, 32, 16, EyeL, 128, 1.0); // Left eye (phase offset)
  // Nose with finer pattern
  drawPlasmaXbm(56, 14, 8, 8, nose, 64, 2.0);
  drawPlasmaXbm(64, 14, 8, 8, noseL, 64, 2.0);
  // Mouth with larger pattern

  // drawPlasmaXbm(0, 16, 64, 16, maw, 32, 2.0);
  // drawPlasmaXbm(64, 16, 64, 16, mawL, 32, 2.0);

  drawPlasmaXbm(0, 10, 64, 22, maw2Closed, 32, 0.5);
  drawPlasmaXbm(64, 10, 64, 22, maw2ClosedL, 32, 0.5);
}

void updatePlasmaFace()
{
  static unsigned long lastUpdate = 0;
  static unsigned long lastPaletteChange = 0;
  const uint16_t frameDelay = 1;               // Delay for plasma animation update
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

// Array of all the bitmaps (Total bytes used to store images in PROGMEM = Cant remember)
//  Create code to list available bitmaps for future bluetooth control

int current_view = 4;
static int number_of_views = 12;

static char view_name[12][30] = {
    "nose",
    "maw",
    "Glitch1",
    "Glitch2",
    "Eye",
    "Angry",
    "Spooked",
    "vwv",
    "blush",
    "semicircleeyes",
    "x_eyes",
    "slanteyes"};

static char *view_bits[12] = {
    nose,
    maw,
    Glitch1,
    Glitch2,
    Eye,
    Angry,
    Spooked,
    vwv,
    blush,
    semicircleeyes,
    x_eyes,
    slanteyes
    // spiral
};

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

void updateBlinkAnimationOld() {
  static bool initialized = false;
  if (!initialized) {
      initBlinkState();
      initialized = true;
  }
  const unsigned long now = millis();

  if (isBlinking) {
      const unsigned long elapsed = now - blinkState.startTime;
      const unsigned long closeDur = blinkState.closeDuration;
      const unsigned long holdDur  = blinkState.holdDuration;
      const unsigned long openDur  = blinkState.openDuration;
      const unsigned long totalDuration = closeDur + holdDur + openDur;

      // Safety and validity checks: terminate blink if too much time has elapsed or if phase durations are invalid.
      if (elapsed > blinkState.durationMs * 2 || totalDuration == 0) {
          isBlinking = false;
          setBlinkProgress(0);
          lastEyeBlinkTime = now;
          nextBlinkDelay = random(minBlinkDelay, maxBlinkDelay);
          return;
      }
      
      // Pre-calculate phase thresholds.
      const unsigned long phase1End = closeDur;             // End of closing phase.
      const unsigned long phase2End = closeDur + holdDur;     // End of hold phase.

      if (elapsed < phase1End) {
          // Closing phase with integer ease-in.
          const unsigned long scaledProgress = (elapsed * 100UL) / closeDur;
          const unsigned long eased = (scaledProgress * scaledProgress + 50UL) / 100UL;
          setBlinkProgress(static_cast<int>(eased > 100UL ? 100UL : eased));
      }
      else if (elapsed < phase2End) {
          // Hold phase – full blink.
          setBlinkProgress(100);
      }
      else if (elapsed < totalDuration) {
          // Opening phase with integer ease-out.
          const unsigned long openElapsed = elapsed - phase2End;
          const unsigned long scaledProgress = (openElapsed * 100UL) / openDur;
          const unsigned long inv = (scaledProgress > 100UL) ? 0UL : (100UL - scaledProgress);
          const unsigned long eased = 100UL - ((inv * inv + 50UL) / 100UL);
          setBlinkProgress(static_cast<int>(eased > 100UL ? 100UL : eased));
      }
      else {
          // Blink cycle complete; reset variables.
          isBlinking = false;
          setBlinkProgress(0);
          lastEyeBlinkTime = now;
          nextBlinkDelay = random(minBlinkDelay, maxBlinkDelay);
      }
  }
  else if (now - lastEyeBlinkTime >= nextBlinkDelay) {
    isBlinking = true;
    blinkState.startTime = now;
    blinkState.durationMs = random(minBlinkDuration, maxBlinkDuration);
    blinkState.closeDuration = blinkState.durationMs * 0.30;
    blinkState.holdDuration = blinkState.durationMs * 0.15;
    blinkState.openDuration = blinkState.durationMs * 0.55;
    blinkProgress = 0;
  }
}

void updateBlinkAnimation() {
 const unsigned long now = millis();

    // Check if it's time to start a new blink
    if (manualBlinkTrigger || (!isBlinking && (now - lastEyeBlinkTime >= nextBlinkDelay))) {
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
    if (isBlinking) {
        const unsigned long elapsed = now - blinkState.startTime;
        const unsigned long totalDuration = blinkState.closeDuration + blinkState.holdDuration + blinkState.openDuration;

        if (elapsed < blinkState.closeDuration) {
            float t = (float)elapsed / blinkState.closeDuration;
            blinkProgress = 100 * easeInQuad(t);
        } else if (elapsed < blinkState.closeDuration + blinkState.holdDuration) {
            blinkProgress = 100;
        } else if (elapsed < totalDuration) {
            float t = (float)(elapsed - blinkState.closeDuration - blinkState.holdDuration) / blinkState.openDuration;
            blinkProgress = 100 * (1.0f - easeOutQuad(t));
        } else {
            // Blink finished
            isBlinking = false;
            blinkProgress = 0;
            lastEyeBlinkTime = now;
            // Calculate delay for the next blink
            nextBlinkDelay = minBlinkDelay + random(maxBlinkDelay - minBlinkDelay + 1);
            // 15% chance of a quick "double blink"
            if (random(100) < 15) {
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



void updateRotatingSpiral() {
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
void blinkingEyes() {
  // Combine bounce and idle hover offsets
  int final_y_offset = currentEyeYOffset + idleEyeYOffset;
  int final_x_offset = idleEyeXOffset;

  // --- Eye Configuration ---
  const uint8_t* rightEyeBitmap = (const uint8_t*)slanteyes; // Default
  const uint8_t* leftEyeBitmap = (const uint8_t*)slanteyesL;   // Default
  int eyeWidth = 24, eyeHeight = 16;
  int rightEyeX = 2, rightEyeY = 0;
  int leftEyeX = 102, leftEyeY = 0;
  bool usePlasma = true; // Most views use plasma
  uint16_t solidColor = dma_display->color565(0, 255, 255); // Default cyan, not used if usePlasma is true

  // Select eye assets and properties based on the current view
  switch (currentView) {
    case 4: // Normal
    case 5: // Blush
      rightEyeBitmap = (const uint8_t*)Eye;
      leftEyeBitmap = (const uint8_t*)EyeL;
      eyeWidth = 32; eyeHeight = 16;
      rightEyeX = 0; rightEyeY = 0;
      leftEyeX = 96; leftEyeY = 0;
      break;
    case 6: // Semicircle
      rightEyeBitmap = (const uint8_t*)semicircleeyes;
      leftEyeBitmap = (const uint8_t*)semicircleeyes; // Same bitmap, different plasma offset
      eyeWidth = 32; eyeHeight = 12;
      rightEyeX = 2; rightEyeY = 2;
      leftEyeX = 94; leftEyeY = 2;
      break;
    case 7: // X eyes
      rightEyeBitmap = (const uint8_t*)x_eyes;
      leftEyeBitmap = (const uint8_t*)x_eyes; // Same bitmap
      eyeWidth = 31; eyeHeight = 15;
      rightEyeX = 0; rightEyeY = 0;
      leftEyeX = 96; leftEyeY = 0;
      break;
    case 8: // Slant eyes (This is also the default)
      // Values are already set by default
      break;
    case 9: // Spiral eyes
      return; // Spiral view handles its own drawing, so we exit here.
    case 17: // Circle eyes
      rightEyeBitmap = (const uint8_t*)circleeyes;
      leftEyeBitmap = (const uint8_t*)circleeyes; // Same bitmap
      eyeWidth = 25; eyeHeight = 21;
      rightEyeX = 10; rightEyeY = 2;
      leftEyeX = 93; leftEyeY = 2;
      break;
  // Default case uses the slanteyes defined before the switch

  }

  // Redundant direct draw branches removed — the switch above sets the correct bitmaps/positions
  // and the subsequent drawBitmapAdvanced calls handle drawing for the selected view.

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
  // Clear the blush area to ensure the effect is removed
  dma_display->fillRect(45, 1, 18, 13, 0); // Clear right blush area
  dma_display->fillRect(72, 1, 18, 13, 0); // Clear left blush area
}

// Update the blush effect state (non‑blocking)
void updateBlush()
{
  unsigned long now = millis();
  unsigned long elapsed = now - blushStateStartTime;

  switch (blushState)
  {
  case BLUSH_FADE_IN:
    if (elapsed < fadeInDuration)
    {
      // Use ease-in for a smooth start
      float progress = (float)elapsed / fadeInDuration;
      blushBrightness = 255 * easeInQuad(progress);
    }
    else
    {
      blushBrightness = 255;
      blushState = BLUSH_FULL;
      blushStateStartTime = now; // Restart timer for full brightness phase
    }
    break;

  case BLUSH_FULL:
    if (elapsed >= fullDuration)
    {
      // After full brightness time, start fading out
      blushState = BLUSH_FADE_OUT;
      blushStateStartTime = now; // Restart timer for fade-out
    }
    // Brightness remains at 255 during this phase.
    break;

  case BLUSH_FADE_OUT:
    if (elapsed < fadeOutDuration)
    {
      // Use ease-out for a smooth end
      float progress = (float)elapsed / fadeOutDuration;
      blushBrightness = 255 * (1.0f - easeOutQuad(progress));
    }
    else
    {
      blushBrightness = 0;
      blushState = BLUSH_INACTIVE;
      disableBlush();

      // Revert to previous view if this was an overlay blush
      if (wasBlushOverlay) {
          currentView = originalViewBeforeBlush;
          saveLastView(currentView);
          Serial.printf("Blush overlay finished. Reverting to view: %d\n", currentView);
          if (deviceConnected) {
              xTaskNotifyGive(bleNotifyTaskHandle);
          }
          wasBlushOverlay = false; // Reset the flag
      }
    }
    break;

  default:
    wasBlushOverlay = false; // Ensure flag is reset if state becomes inactive
    break;
  }
}

void drawBlush() {
  // Serial.print("Blush brightness: ");
  // Serial.println(blushBrightness);

  // Set blush color based on brightness
  uint16_t blushColor = dma_display->color565(blushBrightness, 0, blushBrightness);

  // Clear only the blush area to prevent artifacts
  dma_display->fillRect(45, 1, 18, 13, 0); // Clear right blush area
  dma_display->fillRect(72, 1, 18, 13, 0); // Clear left blush area

  drawXbm565(45, 1, 11, 13, blush, blushColor);
  drawXbm565(40, 1, 11, 13, blush, blushColor);
  drawXbm565(35, 1, 11, 13, blush, blushColor);
  drawXbm565(72, 1, 11, 13, blushL, blushColor);
  drawXbm565(77, 1, 11, 13, blushL, blushColor);
  drawXbm565(82, 1, 11, 13, blushL, blushColor);
}

void drawTransFlag() {
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

void baseFace() {
  // Combine all offsets for final positioning.
  // These will be used for any facial feature that should move with the hover/bounce effect.
  int final_y_offset = currentEyeYOffset + idleEyeYOffset;
  int final_x_offset = idleEyeXOffset;

  blinkingEyes(); // This function now correctly uses the global offsets internally

  if (mouthOpen) {
    drawPlasmaXbm(0, 10, 64, 22, maw2, 0, 1.0);
    drawPlasmaXbm(64, 10, 64, 22, maw2L, 128, 1.0);   // Left eye open (phase offset)
  } else {
    drawPlasmaXbm(0, 10, 64, 22, maw2Closed, 0, 1.0);     // Right eye
    drawPlasmaXbm(64, 10, 64, 22, maw2ClosedL, 128, 1.0); // Left eye (phase offset)
  }

  drawPlasmaXbm(56, 4 + final_y_offset, 8, 8, nose, 64, 2.0);
  drawPlasmaXbm(64, 4 + final_y_offset, 8, 8, noseL, 64, 2.0);

  if (currentView > 3)
  { // Only draw blush effect for face views, not utility views
    if (blushState != BLUSH_INACTIVE) { // Only draw if blush is active
      drawBlush();
    }
  }
  facePlasmaDirty = false;
}

void patternPlasma() {
  static uint16_t paletteLUT[256];
  static CRGBPalette16 cachedPalette;
  static uint16_t cachedBrightnessScale = 0;
  static bool paletteLUTValid = false;

  if (!paletteLUTValid || cachedBrightnessScale != globalBrightnessScaleFixed || std::memcmp(&cachedPalette, &currentPalette, sizeof(CRGBPalette16)) != 0) {
    const uint16_t scale = globalBrightnessScaleFixed;
    for (int i = 0; i < 256; ++i) {
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
  const uint16_t time_val = time_counter; // Use a local copy for calculations
  const uint16_t term2_factor = 128 - wibble; // Calculate 128 - sin8(t) once

  // Get display dimensions once
  const int display_width = dma_display->width();
  const int display_height = dma_display->height();

  // Outer loop for X
  for (int x = 0; x < display_width; x++) {
      // Pre-calculate terms dependent only on X and time
      const uint16_t term1_base = x * wibble * 3 + time_val;
      // The y*x part needs to be inside the Y loop, but we can precalculate x * cos8_val_shifted
      const uint16_t term3_x_factor = x * cos8_val_shifted;

      // Inner loop for Y
      for (int y = 0; y < display_height; y++) {
          // Calculate plasma value 'v'
          // Start with base offset
          int16_t v = 128;
          // Add terms using pre-calculated values where possible
          v += sin16(term1_base);                   // sin16(x*wibble*3 + t)
          v += cos16(y * term2_factor + time_val); // cos16(y*(128-wibble) + t)
          v += sin16(y * term3_x_factor);          // sin16(y * (x * cos8(-t) >> 3))

          const uint16_t color565 = paletteLUT[static_cast<uint8_t>(v >> 8)];
          dma_display->drawPixel(x, y, color565);
      }
  }
}


void displaySleepMode() {
  static unsigned long lastBlinkTime = 0;
  static bool eyesOpen = false;
  static float animationPhase = 0;
  static unsigned long lastAnimTime = 0;

  // Apply brightness adjustment for breathing effect
  static unsigned long lastBreathTime = 0;
  static float brightness = 0;
  static float breathingDirection = 1; // 1 for increasing, -1 for decreasing

  // Update breathing effect
  if (millis() - lastBreathTime >= 50) {
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

  if (eyesOpen) {
    // Draw slightly open eyes - just a small slit
    dma_display->fillRect(5, 5, 20, 2, dma_display->color565(150, 150, 150));
    dma_display->fillRect(103, 5, 20, 2, dma_display->color565(150, 150, 150));
  } else {
    // Draw closed eyes
    dma_display->drawLine(5, 10, 40, 12, dma_display->color565(150, 150, 150));
    //dma_display->drawLine(83, 10, 40, 12, dma_display->color565(150, 150, 150));
    dma_display->drawLine(88, 12, 123, 10, dma_display->color565(150,150,150));
  }
  // Draw sleeping mouth (slight curve)
  /*
  drawXbm565(0, 20, 10, 8, maw2Closed, dma_display->color565(120, 120, 120));
  drawXbm565(64, 20, 10, 8, maw2ClosedL, dma_display->color565(120, 120, 120));
*/
  dma_display->drawFastHLine(PANE_WIDTH/2 - 15, PANE_HEIGHT - 8, 30, dma_display->color565(120,120,120));
  dma_display->flipDMABuffer();
}

void initStarfield() {
  for (int i = 0; i < NUM_STARS; i++) {
      stars[i].x = random(0, dma_display->width());
      stars[i].y = random(0, dma_display->height());
      stars[i].speed = random(1, 4); // Stars move at different speeds
  }
}

void updateStarfield() {
  // Clear the display for a fresh frame
  dma_display->clearScreen(); // Uncommented to clear the screen
  // Update and draw each star
  for (int i = 0; i < NUM_STARS; i++) {
      dma_display->drawPixel(stars[i].x, stars[i].y, dma_display->color565(255, 255, 255));
      // Move star leftwards based on its speed
      stars[i].x -= stars[i].speed;
      // Reset star to right edge if it goes off the left side
      if (stars[i].x < 0) {
          stars[i].x = dma_display->width() - 1;
          stars[i].y = random(0, dma_display->height());
          stars[i].speed = random(1, 4);
      }
  }
 //dma_display->flipDMABuffer();
}

void initDVDLogoAnimation() {
  logos[0] = {0, 0, 1, 1, dma_display->color565(255, 0, 255)};    // Left panel
  logos[1] = {64, 0, -1, 1, dma_display->color565(0, 255, 255)};  // Right panel
}

void bleNotifyTask(void *param) {
    for (;;) {
      // block here until someone calls xTaskNotifyGive()
      ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
      if (deviceConnected) {
        uint8_t bleViewValue = static_cast<uint8_t>(currentView);
       pFaceCharacteristic->setValue(&bleViewValue, 1);
       pFaceCharacteristic->notify();
       pConfigCharacteristic->notify();
      }
    }
 }

// Add this at the top, just after your other declarations:
static void displayTask(void* pvParameters) {
  for(;;) {
    // render the current view and flip DMA buffer
    displayCurrentView(currentView);
    vTaskDelay(pdMS_TO_TICKS(targetFrameIntervalMillis));
  }
}

const i2s_port_t I2S_PORT = I2S_NUM_0;

static volatile unsigned long softMillis = 0;  // our software “millis” counter
TimerHandle_t softMillisTimer = nullptr;

// this callback fires every 1 ms and increments softMillis:
static void IRAM_ATTR onSoftMillisTimer(TimerHandle_t xTimer) {
  (void)xTimer;
  softMillis++;
}


// Matrix and Effect Configuration
#define MAX_FPS 45 // Maximum redraw rate, frames/second

#ifndef PDUST_N_COLORS // Allow overriding from a global config if needed
#define PDUST_N_COLORS 8
#endif

#define N_COLORS   PDUST_N_COLORS // Number of distinct colors for sand
#define BOX_HEIGHT 8              // Initial height of each color block of sand

// Calculate total number of sand grains.
// Logic: Each color forms a block. There are N_COLORS blocks.
// Width of each block = WIDTH / N_COLORS
// Number of grains per block = (WIDTH / N_COLORS) * BOX_HEIGHT
// Total grains = N_COLORS * (WIDTH / N_COLORS) * BOX_HEIGHT = WIDTH * BOX_HEIGHT
// Original calculation: (BOX_HEIGHT * N_COLORS * 8). If WIDTH=64, N_COLORS=8, BOX_HEIGHT=8,
// this is (8 * 8 * 8) = 512. And WIDTH * BOX_HEIGHT = 64 * 8 = 512. So they are equivalent for these values.
#define N_GRAINS (PANE_WIDTH * BOX_HEIGHT) // Total number of sand grains
//#define N_GRAINS 10
//#include "pixelDustEffect.h" // Include the header for this effect
//#include "main.h"            // Include your main project header if it exists (e.g., for global configs)

// Define global variables declared as extern in the header
uint16_t colors[N_COLORS];
Adafruit_PixelDust sand(PANE_WIDTH, PANE_HEIGHT, N_GRAINS, 1.0f, 128, false); // elasticity 1.0, accel_scale 128
uint32_t prevTime = 0;

// Initializes the Pixel Dust effect settings
void setupPixelDust() {
    Serial.println("setupPixelDust: Entered function."); // DEBUG

    // Check if dma_display has been initialized
    if (!dma_display) {
        Serial.println("PixelDust Error: dma_display is not initialized before setupPixelDust() call!");
        // This could be a crash point if dma_display is null and then used.
        // However, the check should prevent dereferencing a null pointer.
        // If it crashes *before* this serial print, dma_display might be valid but its internal state is bad.
        return; // Or better: enter an error state/loop
    }
    Serial.println("setupPixelDust: dma_display is not null."); // DEBUG

     if (N_COLORS > 0) colors[0] = dma_display->color565(64, 64, 64);    // Dark Gray
    if (N_COLORS > 1) colors[1] = dma_display->color565(120, 79, 23);   // Brown
    if (N_COLORS > 2) colors[2] = dma_display->color565(228, 3, 3);     // Red
    if (N_COLORS > 3) colors[3] = dma_display->color565(255, 140, 0);   // Orange
    if (N_COLORS > 4) colors[4] = dma_display->color565(255, 237, 0);   // Yellow
    if (N_COLORS > 5) colors[5] = dma_display->color565(0, 128, 38);    // Green
    if (N_COLORS > 6) colors[6] = dma_display->color565(0, 77, 255);    // Blue
    if (N_COLORS > 7) colors[7] = dma_display->color565(117, 7, 135);   // Purple
    // Add more if N_COLORS > 8
    Serial.printf("setupPixelDust: Colors initialized. N_COLORS = %d\n", N_COLORS); // DEBUG

    // Set up initial sand coordinates in colored blocks at the bottom of the matrix
    int grain_idx = 0;

    // CRITICAL: Potential division by zero or incorrect logic if N_COLORS is 0 or PANE_WIDTH is small
    if (N_COLORS == 0) {
        Serial.println("PixelDust Error: N_COLORS is 0 in setupPixelDust! Cannot proceed.");
        return;
    }
    int grains_per_block_width = PANE_WIDTH / N_COLORS; // Ensure PANE_WIDTH is used here
    Serial.printf("setupPixelDust: PANE_WIDTH=%d, N_COLORS=%d, grains_per_block_width=%d\n", PANE_WIDTH, N_COLORS, grains_per_block_width); // DEBUG

    if (grains_per_block_width == 0 && N_GRAINS > 0) { // If N_COLORS > PANE_WIDTH
        Serial.printf("PixelDust Warning: grains_per_block_width is 0! (PANE_WIDTH=%d, N_COLORS=%d). Grains may not be placed correctly.\n", PANE_WIDTH, N_COLORS);
        // This configuration is problematic. You might want to return or handle it differently.
    }

    for (int i = 0; i < N_COLORS; i++) { // Iterate through each color block
        int block_start_x = i * grains_per_block_width;
        // Ensure PANE_HEIGHT is used here
        int block_start_y = PANE_HEIGHT - BOX_HEIGHT;
        Serial.printf("setupPixelDust: Color block %d: start_x=%d, start_y=%d\n", i, block_start_x, block_start_y); // DEBUG

        for (int y_in_block = 0; y_in_block < BOX_HEIGHT; y_in_block++) {
            for (int x_in_block = 0; x_in_block < grains_per_block_width; x_in_block++) {
                if (grain_idx < N_GRAINS) {
                    // CRITICAL: This is where `sand.setPosition()` is called.
                    // If the `sand` object itself is not properly initialized (e.g., memory allocation failed
                    // when `Adafruit_PixelDust sand(...)` was declared), or if the coordinates
                    // (block_start_x + x_in_block, block_start_y + y_in_block) are outside the
                    // dimensions specified when `sand` was created (PANE_WIDTH, PANE_HEIGHT),
                    // this could cause a crash.
                    int current_x_pos = block_start_x + x_in_block;
                    int current_y_pos = block_start_y + y_in_block;

                    // Add boundary checks for the coordinates passed to setPosition
                    if (current_x_pos < 0 || current_x_pos >= PANE_WIDTH ||
                        current_y_pos < 0 || current_y_pos >= PANE_HEIGHT) {
                        Serial.printf("PixelDust Error: Calculated position for grain %d is out of bounds! (%d, %d) for display (%d x %d)\n",
                                      grain_idx, current_x_pos, current_y_pos, PANE_WIDTH, PANE_HEIGHT);
                        // Decide how to handle this: skip, clamp, or error out
                        // For now, let's skip to avoid crashing Adafruit_PixelDust
                        // grain_idx++; // If you skip, you might need to adjust N_GRAINS or live with fewer grains
                        // continue; // Skip this grain
                    }

                    // Serial.printf("Setting grain %d to (%d, %d)\n", grain_idx, current_x_pos, current_y_pos); // Very verbose debug
                    sand.setPosition(grain_idx, current_x_pos, current_y_pos);
                    grain_idx++;
                } else {
                    // This case should ideally not be reached if N_GRAINS is calculated correctly.
                    // If it is, it means your N_GRAINS calculation might be off or the loop structure is placing more.
                     Serial.printf("PixelDust Warning: grain_idx (%d) exceeded N_GRAINS (%d) during placement.\n", grain_idx, N_GRAINS);
                     // break; // from inner x_in_block loop
                }
            }
            // if (grain_idx >= N_GRAINS && i < N_COLORS -1 ) break; // from y_in_block loop if all grains placed
        }
        // if (grain_idx >= N_GRAINS) break; // from i (color block) loop if all grains placed
    }
    Serial.printf("PixelDust: Initialized %d grains.\n", grain_idx);
    if (grain_idx != N_GRAINS) {
        Serial.printf("PixelDust Warning: Mismatch! Expected %d grains, placed %d. Check N_GRAINS calculation and loop logic.\n", N_GRAINS, grain_idx);
    }
    Serial.println("setupPixelDust: Exiting function."); // DEBUG
}

// Add an enum for clarity (optional, but good practice)
enum SpiralColorMode {
    SPIRAL_COLOR_PALETTE,
    SPIRAL_COLOR_WHITE
};

// Modified function signature:
void updateAndDrawFullScreenSpiral(SpiralColorMode colorMode) { // Added colorMode parameter
    unsigned long currentTime = millis();
    if (currentTime - lastFullScreenSpiralUpdateTime < FULL_SCREEN_SPIRAL_FRAME_INTERVAL_MS) {
        return;
    }
    lastFullScreenSpiralUpdateTime = currentTime;

    // Update animation parameters (rotation, color scroll - color scroll only if palette mode)
    fullScreenSpiralAngle += FULL_SPIRAL_ROTATION_SPEED_RAD_PER_UPDATE;
    if (fullScreenSpiralAngle >= TWO_PI) fullScreenSpiralAngle -= TWO_PI;

    if (colorMode == SPIRAL_COLOR_PALETTE) { // Only update color offset if using palette
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
    for (int step = 0; step < maxSteps; ++step) {
        const int drawX = static_cast<int>(centerX + x + 0.5f);
        const int drawY = static_cast<int>(centerY + y + 0.5f);

        uint16_t pixel_color;
        if (colorMode == SPIRAL_COLOR_WHITE) {
            pixel_color = dma_display->color565(255, 255, 255);
        } else {
            const uint8_t color_index = static_cast<uint8_t>(colorPhase);
            CRGB crgb_color = ColorFromPalette(RainbowColors_p, color_index, 255, LINEARBLEND);
            pixel_color = dma_display->color565(crgb_color.r, crgb_color.g, crgb_color.b);
        }

        if (SPIRAL_THICKNESS_RADIUS == 0) {
            if (drawX >= 0 && drawX < PANE_WIDTH && drawY >= 0 && drawY < PANE_HEIGHT) {
                dma_display->drawPixel(drawX, drawY, pixel_color);
            }
        } else {
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

        if ((x * x + y * y) > maxRadiusSquared) {
            break;
        }
    }
}



void setup() {
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

  // — LOAD LAST VIEW —
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
  //NimBLEDevice::init("LumiFur_Controller");
  NimBLEDevice::init("LF-052618");
  // NimBLEDevice::setPower(ESP_PWR_LVL_P9); // Power level 9 (highest) for best range
  // NimBLEDevice::setPower(ESP_PWR_LVL_P21, NimBLETxPowerType::All); // Power level 21 (highest) for best range
  NimBLEDevice::setPower(ESP_PWR_LVL_P9, NimBLETxPowerType::All); // Power level 21 (highest) for best range
  NimBLEDevice::setSecurityAuth(BLE_SM_PAIR_AUTHREQ_BOND | BLE_SM_PAIR_AUTHREQ_SC);
  pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(&serverCallbacks);

  NimBLEService *pService = pServer->createService(SERVICE_UUID);

  NimBLECharacteristic *pDeviceInfoCharacteristic = pService->createCharacteristic(
      INFO_CHARACTERISTIC_UUID,
      NIMBLE_PROPERTY::READ);

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
    NIMBLE_PROPERTY::WRITE
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
          NIMBLE_PROPERTY::READ |
          NIMBLE_PROPERTY::WRITE |
              NIMBLE_PROPERTY::NOTIFY
          // NIMBLE_PROPERTY::READ_ENC
      );

  // Create a characteristic for configuration.
  pConfigCharacteristic = pService->createCharacteristic(
    CONFIG_CHARACTERISTIC_UUID,
    NIMBLE_PROPERTY::READ |
    NIMBLE_PROPERTY::WRITE |
    //NIMBLE_PROPERTY::WRITE_NR |
    NIMBLE_PROPERTY::NOTIFY
);

// Set the callback to handle writes.
pConfigCharacteristic->setCallbacks(&configCallbacks);

// Optionally, set an initial value.
uint8_t initValue[4] = { 
  static_cast<uint8_t>(autoBrightnessEnabled ? 1 : 0),
  static_cast<uint8_t>(accelerometerEnabled ? 1 : 0),
  static_cast<uint8_t>(sleepModeEnabled ? 1 : 0),
  static_cast<uint8_t>(auroraModeEnabled ? 1 : 0)
};

pConfigCharacteristic->setValue(initValue, sizeof(initValue));
if (pConfigCharacteristic != nullptr) {
Serial.println("Config Characteristic created");
} else {
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
        NIMBLE_PROPERTY::NOTIFY |
        NIMBLE_PROPERTY::WRITE |
      NIMBLE_PROPERTY::WRITE_NR);
pBrightnessCharacteristic->setCallbacks(&brightnessCallbacks);
// initialize with current brightness

pOtaCharacteristic = pService->createCharacteristic(
    OTA_CHARACTERISTIC_UUID,
    NIMBLE_PROPERTY::READ |
        NIMBLE_PROPERTY::WRITE |
        NIMBLE_PROPERTY::WRITE_NR |
        NIMBLE_PROPERTY::NOTIFY);
pOtaCharacteristic->setCallbacks(&otaCallbacks);
NimBLEDescriptor *otaDesc = pOtaCharacteristic->createDescriptor(
    "2901",
    NIMBLE_PROPERTY::READ,
    20);
otaDesc->setValue("OTA Control");
pBrightnessCharacteristic->setValue(&userBrightness, 1);

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
mxconfig.i2sspeed = HUB75_I2S_CFG::HZ_20M;
mxconfig.clkphase = false;
mxconfig.double_buff = true; // <------------- Turn on double buffer

#ifndef VIRTUAL_PANE
  dma_display = new MatrixPanel_I2S_DMA(mxconfig);
  dma_display->begin();
  dma_display->setBrightness8(userBrightness);
  updateGlobalBrightnessScale(userBrightness);
#else
  chain = new MatrixPanel_I2S_DMA(mxconfig);
  chain->begin();
  chain->setBrightness8(userBrightness);
  updateGlobalBrightnessScale(userBrightness);
  // create VirtualDisplay object based on our newly created dma_display object
  matrix = new VirtualMatrixPanel((*chain), NUM_ROWS, NUM_COLS, PANEL_WIDTH, PANEL_HEIGHT, CHAIN_TOP_LEFT_DOWN);
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
    err(250); // Fast blink = I2C error
     g_accelerometer_initialized = false; // Set flag
  }
  else
  {
    Serial.println("Accelerometer found!");
    accel.setRange(LIS3DH_RANGE_4_G); // 4G range is sufficient for motion detection
    //accel.setDataRate(LIS3DH_DATARATE_100_HZ); // Set data rate to 100Hz
    //accel.setPerformanceMode(LIS3DH_MODE_LOW_POWER);
    g_accelerometer_initialized = true; // Set flag on success // Set to low power mode
  }
#endif

  //setupPixelDust();

#if defined(ARDUINO_ADAFRUIT_MATRIXPORTAL_ESP32S3) || defined(ACCEL_AVAILABLE)
  // Ensure dma_display, accel, and accelerometerEnabled are initialized before this line
  fluidEffectInstance = new FluidEffect(dma_display, &accel, &accelerometerEnabled);
#else
  // Pass nullptr for accelerometer if not available/compiled
  fluidEffectInstance = new FluidEffect(dma_display, nullptr, &accelerometerEnabled);
#endif

  if (!fluidEffectInstance) {
    Serial.println("FATAL: Failed to allocate FluidEffect instance!");
    // Handle error appropriately, e.g., by blinking an LED or halting.
    // For now, it will just print "Fluid Error" in displayCurrentView if null.
  }

  // ... rest of your setup() code ...
  // e.g., currentView = getLastView();

  // If the initial view is the fluid effect, call begin()
  if (currentView == 16 && fluidEffectInstance) { // Assuming 16 is the fluid effect view
      fluidEffectInstance->begin();
  }
/*
#if defined(ARDUINO_ADAFRUIT_MATRIXPORTAL_ESP32S3) || defined(ACCEL_AVAILABLE)
    pixelDustEffectInstance = new PixelDustEffect(dma_display, &accel, &accelerometerEnabled);
#else
    pixelDustEffectInstance = new PixelDustEffect(dma_display, nullptr, &accelerometerEnabled);
#endif

if (!pixelDustEffectInstance) {
    Serial.println("FATAL: Failed to allocate PixelDustEffect instance!");
}
// If the initial view is one of the effects, call its begin()
    if (currentView == 16 && fluidEffectInstance) { // Old fluid effect view number
        fluidEffectInstance->begin();
    }
    if (currentView == 17 && pixelDustEffectInstance) { // <<< NEW: Assuming PixelDust is view 17
        pixelDustEffectInstance->begin();
    }
*/

  lastActivityTime = millis(); // Initialize the activity timer for sleep mode

  randomSeed(analogRead(0)); // Seed the random number generator

  // Set sleep timeout based on debug mode
  SLEEP_TIMEOUT_MS = DEBUG_MODE ? SLEEP_TIMEOUT_MS_DEBUG : SLEEP_TIMEOUT_MS_NORMAL;

  // Set initial plasma color palette
  currentPalette = RainbowColors_p;


  snprintf(txt, sizeof(txt), "HAVE A WONDERFUL DAY!");
  initializeScrollingText();

////////Setup Bouncing Squares////////
  myDARK = dma_display->color565(64, 64, 64);
  myWHITE = dma_display->color565(192, 192, 192);
  myRED = dma_display->color565(255, 0, 0);
  myGREEN = dma_display->color565(0, 255, 0);
  myBLUE = dma_display->color565(0, 0, 255);

  colours = {{ myDARK, myWHITE, myRED, myGREEN, myBLUE }};

  // Create some random squares
  for (int i = 0; i < numSquares; i++)
  {
    Squares[i].square_size = random(2,10);
    Squares[i].xpos = random(0, dma_display->width() - Squares[i].square_size);
    Squares[i].ypos = random(0, dma_display->height() - Squares[i].square_size);
    Squares[i].velocityx = static_cast <float> (rand()) / static_cast <float> (RAND_MAX);
    Squares[i].velocityy = static_cast <float> (rand()) / static_cast <float> (RAND_MAX);

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

/*
// start I2S at 16 kHz with 32-bits per sample
if (!I2S.begin(I2S_PHILIPS_MODE, 16000, 32)) {
  Serial.println("Failed to initialize I2S!");
  while (1); // do nothing
}
*/

esp_err_t err_i2s;

i2s_config_t i2sConfig = {
  .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
  //.sample_rate = 22050, // Sample rate
  .sample_rate =  44100, // Sample rate
  .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
  .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
  .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_STAND_I2S | I2S_COMM_FORMAT_STAND_MSB),
  .intr_alloc_flags = 0, // Interrupt level
  .dma_buf_count = 2,                       // Number of DMA buffers
  .dma_buf_len = 32,                      // Length of each DMA buffer
  .use_apll = false,
};
// Configure I2S pins for microphone input (data in only)
i2s_pin_config_t pinConfig = {
  .bck_io_num   = MIC_SCK_PIN,
  .ws_io_num    = MIC_WS_PIN,
  .data_out_num = I2S_PIN_NO_CHANGE,
  .data_in_num  = MIC_SD_PIN
};

err_i2s = i2s_driver_install(I2S_PORT, &i2sConfig, 0, NULL); // Use renamed variable
//REG_SET_BIT(I2S_TIMING_REG(I2S_PORT), BIT(9)); // Set I2S timing register
if (err_i2s != ESP_OK) { // Use renamed variable
  Serial.printf("Failed installing I2S driver: %d\n", err_i2s); // Use renamed variable
  while (true);
}
err_i2s = i2s_set_pin(I2S_PORT, &pinConfig); // Use renamed variable
if (err_i2s != ESP_OK) { // Use renamed variable
  Serial.printf("Failed setting I2S pin: %d\n", err_i2s); // Use renamed variable
  while (true);
}
Serial.println("I2S driver installed.");


// if you need to drive a power line:
pinMode(MIC_PD_PIN, OUTPUT);
digitalWrite(MIC_PD_PIN, HIGH);


#if defined(ARDUINO_ADAFRUIT_MATRIXPORTAL_ESP32S3)
#ifdef BUTTON_UP
  pinMode(BUTTON_UP, INPUT_PULLUP);
#endif
#ifdef BUTTON_DOWN
  pinMode(BUTTON_DOWN, INPUT_PULLUP);
#endif
#endif

  setupAdaptiveBrightness();
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
//#endif

  // Spawn BLE notify task (you already have):
  xTaskCreatePinnedToCore(
      bleNotifyTask,
      "BLE Task",
      4096,
      NULL,
      1,
      &bleNotifyTaskHandle,
      CONFIG_BT_NIMBLE_PINNED_TO_CORE
    );

  // Now spawn the display task pinned to the other core (we'll pick core 1):
  xTaskCreatePinnedToCore(
    displayTask,
    "Display Task",
    8192,               // stack bytes
    NULL,
    3,                  // priority
    NULL,
    1                   // pin to core 1
  );

  // Then return—loop() can be left empty or just do background housekeeping

  // create a 1 ms auto‑reloading FreeRTOS timer
  softMillisTimer = xTimerCreate(
    "softMillis",                 // name
    pdMS_TO_TICKS(1),             // period = 1 ms
    pdTRUE,                       // auto‑reload
    nullptr,                      // timer ID
    onSoftMillisTimer             // callback
  );
  if (softMillisTimer) {
    xTimerStart(softMillisTimer, 0);
  }
}

void drawDVDLogo(int x, int y, uint16_t color) {
  drawXbm565(x, y, dvdWidth, dvdHeight, DvDLogo, color);
}

void updateDVDLogos() {
  unsigned long now = millis();
  if (now - lastDvdUpdate < dvdUpdateInterval) return;
  lastDvdUpdate = now;

  for (int i = 0; i < 2; i++) {
    DVDLogo &logo = logos[i];

    // Clear previous
    dma_display->fillRect(logo.x, logo.y, dvdWidth, dvdHeight, 0);

    // Update position
    logo.x += logo.vx;
    logo.y += logo.vy;

    // Calculate bounds
    int minX = (i == 0) ? 0 : 64;
    int maxX = minX + 64;

    bool bounced = false;

    
// Horizontal bounce (stay within 64x32 region)
if (logo.x < minX) {
  logo.x = minX;
  logo.vx *= -1;
  bounced = true;
} else if (logo.x + dvdWidth > maxX) {
  logo.x = maxX - dvdWidth;
  logo.vx *= -1;
  bounced = true;
}

// Vertical bounce (shared screen height)
if (logo.y < 0) {
  logo.y = 0;
  logo.vy *= -1;
  bounced = true;
} else if (logo.y + dvdHeight > dma_display->height()) {
  logo.y = dma_display->height() - dvdHeight;
  logo.vy *= -1;
  bounced = true;
}
// Change color if any bounce happened
if (bounced) {
  logo.color = dma_display->color565(random(256), random(256), random(256));
}
    // Draw new logo
    drawXbm565(logo.x, logo.y, dvdWidth, dvdHeight, DvDLogo, logo.color);
  }
 //dma_display->flipDMABuffer(); // Flip the buffer to show the updated logos
  // Optional: Add a small delay for smoother animation
}

// Define a threshold for microphone amplitude to trigger mouth open/close
#ifndef MIC_THRESHOLD
#define MIC_THRESHOLD 450000 // Adjust this value as needed for your microphone sensitivity
#endif

static uint8_t mawBrightness = 0; // new: scaled 0–255 based on mic level
void displayCurrentMaw() {
// build a gray‐scale color from current mic level
uint16_t col = dma_display->color565(mawBrightness,
  mawBrightness,
  mawBrightness);
  // Draw open or closed based on the mic‐triggered flag
  if (mouthOpen) {
    #if DEBUG_MODE
    Serial.println("Displaying open maw");
    # endif
    drawXbm565(0, 10, 64, 22, maw2, col);
    drawXbm565(64, 10, 64, 22, maw2L, col);
  } else {
    #if DEBUG_MODE
    Serial.println("Displaying Closed maw");
    #endif
    drawXbm565(0, 10, 64, 22, maw2Closed,  dma_display->color565(255,255,255));
    drawXbm565(64, 10, 64, 22, maw2ClosedL,  dma_display->color565(255,255,255));
  }
}

// ——————————————————————————————————————————————————————————————————————
// Optimized mouth‑movement reader
static int32_t mouthBuf[128];        // static so it lives in .bss, not on the stack

// --- Microphone Processing Parameters ---
// EMA (Exponential Moving Average) alpha for ambient noise (smaller = slower adaptation)
#define AMBIENT_EMA_ALPHA 0.01f
// EMA alpha for signal smoothing (larger = faster response, less smoothing)
#define SIGNAL_EMA_ALPHA 0.3f
// How much louder the signal needs to be than ambient to trigger (absolute value)
#define SENSITIVITY_OFFSET_ABOVE_AMBIENT 340000 // Adjust this based on mic sensitivity and desired responsiveness
                                            // This effectively replaces your old MIC_THRESHOLD's primary role
// Minimum signal level to be considered for ambient noise calculation
#define MIN_SIGNAL_FOR_AMBIENT_CONSIDERATION 500
// Time in ms of silence before updating ambient noise more aggressively
#define AMBIENT_UPDATE_QUIET_PERIOD_MS 1000
// Minimum and maximum clamp for ambient noise estimation
#define MIN_AMBIENT_LEVEL 200   // Prevents ambient from dropping too low
#define MAX_AMBIENT_LEVEL 200000 // Prevents ambient from rising too high due to sustained noise

// --- State Variables (static, so they persist between calls) ---
static float ambientNoiseLevel = (MIN_AMBIENT_LEVEL + MAX_AMBIENT_LEVEL) / 2.0f; // Initial guess
static float smoothedSignalLevel = 0.0f;
static unsigned long lastSoundTime = 0;      // When the mouth was last considered open or sound was high
//static unsigned long lastAmbientUpdateTime = 0;

// Original buffer
//static int32_t mouthBuf[128];

void updateMouthMovement() {
    unsigned long now = millis();
    const bool initialMouthOpen = mouthOpen;
    size_t bytesRead = 0;

    esp_err_t err = i2s_read(
        I2S_PORT,
        mouthBuf,
        sizeof(mouthBuf),

        &bytesRead,
        pdMS_TO_TICKS(10) // Using a small timeout instead of portMAX_DELAY to prevent blocking indefinitely
    );

    if (err != ESP_OK || bytesRead == 0) {
        // Handle error or no data: assume silence
        if (mouthOpen && (now - lastSoundTime > mouthOpenHoldTime)) {
            mouthOpen = false;
        }
        // Keep ambient noise decaying slowly if no signal
        if (now - lastAmbientUpdateTime > 100) { // Update ambient periodically even in silence
             ambientNoiseLevel = ambientNoiseLevel * (1.0f - AMBIENT_EMA_ALPHA) + MIN_AMBIENT_LEVEL * AMBIENT_EMA_ALPHA;
             ambientNoiseLevel = constrain(ambientNoiseLevel, MIN_AMBIENT_LEVEL, MAX_AMBIENT_LEVEL);
             lastAmbientUpdateTime = now;
        }
        smoothedSignalLevel *= (1.0f - SIGNAL_EMA_ALPHA); // Decay smoothed signal

        mawBrightness = 20; // Minimum brightness

        // --- TELEPLOT for no data/error ---

        #if DEBUG_MICROPHONE
        Serial.printf(">mic_avg_abs_signal:0\n");
        Serial.printf(">mic_smoothed_signal:%.0f\n", smoothedSignalLevel);
        Serial.printf(">mic_ambient_noise:%.0f\n", ambientNoiseLevel);
        Serial.printf(">mic_trigger_threshold:%.0f\n", ambientNoiseLevel + SENSITIVITY_OFFSET_ABOVE_AMBIENT);
        Serial.printf(">mic_maw_brightness:%u\n", mawBrightness);
        Serial.printf(">mic_mouth_open:%d\n", mouthOpen ? 1 : 0);
        #endif
        if (mouthOpen != initialMouthOpen) {
            facePlasmaDirty = true;
        }
        return;
        
    }

    size_t samples = bytesRead / sizeof(mouthBuf[0]);
    if (samples == 0) { // Should be caught by bytesRead == 0 above, but as a safeguard
        // (Same logic as above for error/no data)
        if (mouthOpen && (now - lastSoundTime > mouthOpenHoldTime)) {
            mouthOpen = false;
        }
        smoothedSignalLevel *= (1.0f - SIGNAL_EMA_ALPHA);
        mawBrightness = 20;
        #ifdef DEBUG_MIC
        Serial.printf(">mic_avg_abs_signal:0\n");
        Serial.printf(">mic_smoothed_signal:%.0f\n", smoothedSignalLevel);
        # endif
        // ... (rest of teleplot for no samples)
        if (mouthOpen != initialMouthOpen) {
            facePlasmaDirty = true;
        }
        return;
    }

    // 1. Calculate Average Absolute Signal for this frame
    uint32_t sumAbs = 0;
    for (size_t i = 0; i < samples; ++i) {
        // The `>> 8` shift is from your original code.
        // For a typical 18-bit I2S mic (like SPH0645), data is in the upper bits.
        // A more accurate shift might be `>> (32 - 18)` or `>> 14` to get the signed 18-bit value.
        // However, consistency is key for the dynamic threshold.
        sumAbs += (uint32_t)abs(mouthBuf[i] >> 8);
    }
    float currentAvgAbsSignal = (float)sumAbs / samples;

    // 2. Smooth the current signal
    smoothedSignalLevel = SIGNAL_EMA_ALPHA * currentAvgAbsSignal + (1.0f - SIGNAL_EMA_ALPHA) * smoothedSignalLevel;

    // 3. Update Ambient Noise Level
    //    Only update ambient if the current sound is not too loud OR if it's been quiet for a while
    bool isQuietPeriod = (now - lastSoundTime > AMBIENT_UPDATE_QUIET_PERIOD_MS);
    if (isQuietPeriod || smoothedSignalLevel < (ambientNoiseLevel + SENSITIVITY_OFFSET_ABOVE_AMBIENT * 0.5f)) {
        // If it's truly quiet, adapt faster to the current (low) signal
        float currentTargetAmbient = (isQuietPeriod) ? currentAvgAbsSignal : smoothedSignalLevel;
        // Ensure target for ambient isn't excessively high if there was a brief sound
        currentTargetAmbient = min(currentTargetAmbient, ambientNoiseLevel + SENSITIVITY_OFFSET_ABOVE_AMBIENT * 0.2f);


        ambientNoiseLevel = AMBIENT_EMA_ALPHA * currentTargetAmbient + (1.0f - AMBIENT_EMA_ALPHA) * ambientNoiseLevel;
        ambientNoiseLevel = constrain(ambientNoiseLevel, MIN_AMBIENT_LEVEL, MAX_AMBIENT_LEVEL);
        lastAmbientUpdateTime = now;
    }


    // 4. Determine Trigger Threshold
    float triggerThreshold = ambientNoiseLevel + SENSITIVITY_OFFSET_ABOVE_AMBIENT;

    // 5. Decide mouthOpen state
    if (smoothedSignalLevel > triggerThreshold) {
        if (!mouthOpen) { // Rising edge, mouth was closed and now should open
            // Optional: add a slightly higher threshold to OPEN than to STAY open (hysteresis)
            // For now, simple trigger
        }
        mouthOpen = true;
        lastSoundTime = now; // Update last time a significant sound was detected
    } else {
        // Signal is below threshold, check hold time
        if (mouthOpen && (now - lastSoundTime > mouthOpenHoldTime)) {
            mouthOpen = false;
        }
    }

    // 6. Calculate Maw Brightness
    // Scale brightness based on how much the smoothed signal is above the ambient noise
    float signalAboveAmbient = smoothedSignalLevel - ambientNoiseLevel;
    if (signalAboveAmbient < 0) signalAboveAmbient = 0;

    // Map [0, SENSITIVITY_OFFSET_ABOVE_AMBIENT * 2] to [20, 255] for brightness
    // The range for mapping brightness (e.g. SENSITIVITY_OFFSET_ABOVE_AMBIENT * 2 or *3)
    // determines how "loud" a sound needs to be to reach full brightness.
    float brightnessMappingRange = SENSITIVITY_OFFSET_ABOVE_AMBIENT * 2.5f;
    mawBrightness = map((long)(signalAboveAmbient * 100), // Multiply by 100 for map precision
                        0,
                        (long)(brightnessMappingRange * 100),
                        20,  // Min brightness
                        255); // Max brightness
    mawBrightness = constrain(mawBrightness, 20, 255);

    // If mouth is forced closed by hold time, ensure brightness reflects that
    if (!mouthOpen) {
        mawBrightness = 20; // Or a very dim value
    }


    // --- TELEPLOT LINES ---

    #if DEBUG_MICROPHONE 
    Serial.printf(">mic_avg_abs_signal:%.0f\n", currentAvgAbsSignal);
    Serial.printf(">mic_smoothed_signal:%.0f\n", smoothedSignalLevel);
    Serial.printf(">mic_ambient_noise:%.0f\n", ambientNoiseLevel);
    Serial.printf(">mic_trigger_threshold:%.0f\n", triggerThreshold);
    Serial.printf(">mic_maw_brightness:%u\n", mawBrightness);
    Serial.printf(">mic_mouth_open:%d\n", mouthOpen ? 1 : 0);
    # endif

    if (mouthOpen != initialMouthOpen) {
        facePlasmaDirty = true;
    }
}
// Runs one frame of the Pixel Dust animation
void PixelDustEffect() {
    uint32_t t_current;
    #if (DEBUG_MODE) 
    Serial.println("PixelDustEffect: Starting..."); // DEBUG
    #endif

    // Use a static local for prevTime if this is the only place it's used for this specific effect's FPS
    // static uint32_t effect_prev_time = 0;
    // while (((t_current = micros()) - effect_prev_time) < (1000000L / MAX_FPS));
    // effect_prev_time = t_current;
    // OR if prevTime is truly global for this effect:
    while (((t_current = micros()) - prevTime) < (1000000L / MAX_FPS)) {
        // yield or delay briefly if you suspect this loop is too tight for some reason,
        // though usually it's fine.
        // taskYIELD();
    }
    prevTime = t_current;
    Serial.println("PixelDustEffect: FPS throttle passed."); // DEBUG

    if (!dma_display) {
        Serial.println("PixelDust Error: dma_display is null!");
        return;
    }
    Serial.println("PixelDustEffect: dma_display OK."); // DEBUG

    Serial.printf("PixelDustEffect: accelEnabled=%d, g_accel_init=%d\n", accelerometerEnabled, g_accelerometer_initialized); // DEBUG

    if (accelerometerEnabled) {
        if (g_accelerometer_initialized) {
            sensors_event_t event;
            Serial.println("PixelDustEffect: Attempting accel.getEvent()"); // DEBUG
            if (accel.getEvent(&event)) {
                Serial.println("PixelDustEffect: accel.getEvent() SUCCESS."); // DEBUG
                int16_t ax_scaled = (int16_t)(event.acceleration.x * 1000.0);
                int16_t ay_scaled = (int16_t)(event.acceleration.y * 1000.0);
                Serial.println("PixelDustEffect: Before sand.iterate (with accel data)"); // DEBUG
                sand.iterate(ax_scaled, ay_scaled, 0);
                Serial.println("PixelDustEffect: After sand.iterate (with accel data)"); // DEBUG
            } else {
                Serial.println("PixelDust: Failed to get accelerometer event.");
                Serial.println("PixelDustEffect: Before sand.iterate (default gravity after accel fail)"); // DEBUG
                sand.iterate(0, (int16_t)(0.1 * 128), 0);
                Serial.println("PixelDustEffect: After sand.iterate (default gravity after accel fail)"); // DEBUG
            }
        } else {
            Serial.println("PixelDustEffect: Accelerometer hardware not initialized. Using default gravity."); // DEBUG
            Serial.println("PixelDustEffect: Before sand.iterate (default gravity, accel not init)"); // DEBUG
            sand.iterate(0, (int16_t)(0.1 * 128), 0);
            Serial.println("PixelDustEffect: After sand.iterate (default gravity, accel not init)"); // DEBUG
        }
    } else {
        Serial.println("PixelDustEffect: Accelerometer use disabled by config. Using default gravity."); // DEBUG
        Serial.println("PixelDustEffect: Before sand.iterate (default gravity, accel disabled)"); // DEBUG
        sand.iterate(0, (int16_t)(0.1 * 128), 0);
        Serial.println("PixelDustEffect: After sand.iterate (default gravity, accel disabled)"); // DEBUG
    }

    // dma_display->clearScreen(); // is called by displayCurrentView before this effect

    dimension_t px, py;
    int grains_per_block_width = PANE_WIDTH / N_COLORS; // Assuming N_COLORS != 0
    int grains_per_color_stripe = grains_per_block_width * BOX_HEIGHT;

    Serial.printf("PixelDustEffect: N_GRAINS=%d, grains_per_color_stripe=%d\n", N_GRAINS, grains_per_color_stripe); // DEBUG

    if (grains_per_color_stripe == 0 && N_GRAINS > 0) {
        Serial.println("PixelDust Error: grains_per_color_stripe is zero.");
        for (int i = 0; i < N_GRAINS; i++) {
            // Serial.printf("PixelDustEffect: Loop1 - Grain %d / %d\n", i, N_GRAINS); // VERBOSE
            sand.getPosition(i, &px, &py);
            if (px >= 0 && px < dma_display->width() && py >= 0 && py < dma_display->height()) {
                dma_display->drawPixel(px, py, colors[0]);
            }
        }
    } else {
        for (int i = 0; i < N_GRAINS; i++) {
            // Serial.printf("PixelDustEffect: Loop2 - Grain %d / %d\n", i, N_GRAINS); // VERBOSE
            if (i % 100 == 0 && i > 0) { // Print progress less frequently
                 Serial.printf("PixelDustEffect: Processing grain %d\n", i); // DEBUG
            }
            sand.getPosition(i, &px, &py); // Potential crash
            int color_index = (grains_per_color_stripe > 0) ? (i / grains_per_color_stripe) : 0;
            if (color_index >= N_COLORS) color_index = N_COLORS - 1;

            if (px >= 0 && px < dma_display->width() && py >= 0 && py < dma_display->height()) {
                dma_display->drawPixel(px, py, colors[color_index]);
            }
        }
    }
    Serial.println("PixelDustEffect: Drawing loop finished."); // DEBUG
    // No flipDMABuffer here, it's handled by the displayTask

    // Ensure this function matches any prototype you might have in a header file.
}



void displayCurrentView(int view) {
  static int previousViewLocal = -1;           // Track the last active view
  static uint8_t colorWheelOffset = 0; // For scrolling text color

  // If we're in sleep mode, don't display the normal view
  if (sleepModeActive) {
    displaySleepMode(); // This function handles its own flipDMABuffer or drawing rate
    return;
  }
  dma_display->clearScreen(); // Clear the display

  if (view != previousViewLocal)
  { // Check if the view has changed
    facePlasmaDirty = true;
    if (view == 5)
    {
      // Reset fade logic when entering the blush view
      blushStateStartTime = millis();
      isBlushFadingIn = true;
      blushState = BLUSH_FADE_IN; // Start fade‑in state
      #if DEBUG_MODE
      Serial.println("Entered blush view, resetting fade logic");
      #endif
    }
     if (view == 16) { // If switching to Fluid Animation view
      if (fluidEffectInstance) {
        fluidEffectInstance->begin(); // Reset fluid effect state
      }
    }
  }

  switch (view) {

  case VIEW_DEBUG_SQUARES: // Scrolling text Debug View
    // Every frame, we clear the background and draw everything anew.
    // This happens "in the background" with double buffering, that's
    // why you don't see everything flicker. It requires double the RAM,
    // so it's not practical for every situation.
    
    //dma_display->clearScreen(); // Clear the display

    // Update text position for next frame. If text goes off the
    // left edge, reset its position to be off the right edge.
    for (int i = 0; i < numSquares; i++)
  {
    // Draw rect and then calculate
    dma_display->fillRect(Squares[i].xpos, Squares[i].ypos, Squares[i].square_size, Squares[i].square_size, Squares[i].colour);

    if (Squares[i].square_size + Squares[i].xpos >= dma_display->width()) {
      Squares[i].velocityx *= -1;
    } else if (Squares[i].xpos <= 0) {
      Squares[i].velocityx = abs (Squares[i].velocityx);
    }

    if (Squares[i].square_size + Squares[i].ypos >= dma_display->height()) {
      Squares[i].velocityy *= -1;
    } else if (Squares[i].ypos <= 0) {
      Squares[i].velocityy = abs (Squares[i].velocityy);
    }

    Squares[i].xpos += Squares[i].velocityx;
    Squares[i].ypos += Squares[i].velocityy;
  }
    break;

  case VIEW_LOADING_BAR: // Loading bar effect
  //  Draw Apple Logos
  drawXbm565(23, 2, 18, 21, appleLogoApple_logo_black, dma_display->color565(255, 255, 255));
  drawXbm565(88, 2, 18, 21, appleLogoApple_logo_black, dma_display->color565(255, 255, 255));

    displayLoadingBar();
    loadingProgress++;
    if (loadingProgress > loadingMax)
    {
      loadingProgress = 0; // Reset smoothly
    }
    break;

  case VIEW_PATTERN_PLASMA: // Pattern plasma
    patternPlasma();
    //delay(10); // Short delay for smoother animation
    break;

  case VIEW_TRANS_FLAG:
    drawTransFlag();
    break;

  case VIEW_NORMAL_FACE:
    baseFace();
    updatePlasmaFace();
    //delay(10);              // Short delay for smoother animation
    break;

  case VIEW_BLUSH_FACE: // Blush with fade in effect
    baseFace();
    updatePlasmaFace();
    break;

  case VIEW_SEMICIRCLE_EYES: // Dialated pupils
    baseFace();
    updatePlasmaFace();
    break;

  case VIEW_X_EYES: // X eyes
    baseFace();
    updatePlasmaFace();
    break;

  case VIEW_SLANT_EYES: // Slant eyes
    baseFace();
    updatePlasmaFace();
    break;

  case VIEW_SPIRAL_EYES: // Spiral eyes
    updatePlasmaFace();
    baseFace();
    updateRotatingSpiral();
    break;

  case VIEW_PLASMA_FACE: // Plasma bitmap test
    drawPlasmaFace();
    updatePlasmaFace();
    break;

  case VIEW_UWU_EYES: // UwU eyes
    baseFace();
    updatePlasmaFace();
    break;

  case VIEW_STARFIELD: // Starfield
    updateStarfield();
    break;

  case VIEW_BSOD:                                                           // BSOD
    dma_display->fillScreen(dma_display->color565(0, 0, 255));       // Blue screen
    dma_display->setTextColor(dma_display->color565(255, 255, 255)); // White text
    dma_display->setTextSize(2);                                     // Set text size
    dma_display->setCursor(5, 15);
    dma_display->print(":(");
    dma_display->setCursor(69, 15);
    dma_display->print(":(");
    break;

  case VIEW_DVD_LOGO: // DvD Logo
    updateDVDLogos();
    break;

  case VIEW_FLAME_EFFECT: // Flame effect
    //updateAndDrawFlameEffect();
    break;

  case VIEW_FLUID_EFFECT: // Fluid Animation
  /*
    if (fluidEffectInstance)
    {
      fluidEffectInstance->updateAndDraw();
    }
    else
    {
      // Fallback message if fluidEffectInstance is null
      dma_display->fillRect(0, 0, dma_display->width(), dma_display->height(), 0); // Clear
      dma_display->setFont(&TomThumb);                                             // Ensure TomThumb is available
      dma_display->setTextSize(1);
      dma_display->setTextColor(dma_display->color565(255, 0, 0));
      dma_display->setCursor(5, 15);
      dma_display->print("Fluid Err");
      */
     break;

  case VIEW_CIRCLE_EYES: // Circle eyes
    baseFace();
    updatePlasmaFace();
    break;

case VIEW_FULLSCREEN_SPIRAL_PALETTE: //Spiral view
    updateAndDrawFullScreenSpiral(SPIRAL_COLOR_PALETTE); // Call with palette mode
    break;

case VIEW_FULLSCREEN_SPIRAL_WHITE:
    updateAndDrawFullScreenSpiral(SPIRAL_COLOR_WHITE);   // Call with white mode
    // case 17: // Pixel Dust Effect (new view number)
    // PixelDustEffect();
    break;


case VIEW_SCROLLING_TEXT: // Scrolling text view
    drawText(colorWheelOffset++);
    break;

  default:
    // Optional: Handle unsupported views
    // dma_display->clearScreen();
    break;
  }
  // Only flip buffer if not spiral view (spiral handles its own flip)
  // if (view != 9)

#ifdef DEBUG_VIEWS

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
  dma_display->setTextSize(1); // Set text size for FPS counter
  dma_display->setFont(&TomThumb); // Use default font
  dma_display->setTextColor(dma_display->color565(255, 255, 255)); // White text
  // Position the counter at a corner (adjust as needed)
  dma_display->setCursor(37, 5); // Adjust position
  dma_display->print(fpsText);
  // --- End FPS counter overlay ---

#endif


  if (!sleepModeActive) {
    dma_display->flipDMABuffer();
}
    
}

// Helper function to get the current threshold value
float getCurrentThreshold() {
  return useShakeSensitivity ? SHAKE_THRESHOLD : SLEEP_THRESHOLD;
}

// Add with your other utility functions
bool detectMotion() {
  if (!g_accelerometer_initialized) return false; // Guard against uninitialized accelerometer
  accel.read();

float x = accel.x_g; // Use pre-scaled G values if available, e.g., from LIS3DH library
  float y = accel.y_g;
  float z = accel.z_g;

  // Calculate the movement delta from last reading
  float deltaX = fabs(x - prevAccelX);
  float deltaY = fabs(y - prevAccelY);
  float deltaZ = fabs(z - prevAccelZ);

  // Store current values for next comparison
  prevAccelX = x;
  prevAccelY = y;
  prevAccelZ = z;

  // Use the current threshold based on the context
  float threshold = getCurrentThreshold();

  // Check if movement exceeds threshold
  if (deltaX > threshold ||
      deltaY > threshold ||
      deltaZ > threshold)
  {
    return true; // Motion detected
  }
  return false; // No motion detected above threshold
}

void enterSleepMode() {
  if (sleepModeActive) return; // Already sleeping
  Serial.println("Entering sleep mode");
  sleepModeActive = true;
  preSleepView = currentView; // Save current view
  dma_display->setBrightness8(sleepBrightness); // Lower display brightness
  updateGlobalBrightnessScale(sleepBrightness);

  reduceCPUSpeed(); // Reduce CPU speed for power saving

  sleepFrameInterval = 100; // 10 FPS during sleep to save power

  /// Reduce BLE activity
  if (!deviceConnected) {
    NimBLEDevice::getAdvertising()->stop(); // Stop normal advertising
    vTaskDelay(pdMS_TO_TICKS(10)); // Short delay
    NimBLEDevice::getAdvertising()->setMinInterval(2400); // 1500 ms
    NimBLEDevice::getAdvertising()->setMaxInterval(4800); // 3000 ms
    NimBLEDevice::startAdvertising();
     Serial.println("Reduced BLE Adv interval for sleep.");
  } else {
      // Maybe send a sleep notification?
       if (pTemperatureCharacteristic != nullptr) { // Check if characteristic exists
            char sleepMsg[] = "Sleep";
            pTemperatureCharacteristic->setValue(sleepMsg);
            xTaskNotifyGive(bleNotifyTaskHandle);
            //pTemperatureCharacteristic->notify(); // Consider notifyPending
       }
  }
  // No need to change sleepFrameInterval here, main loop timing controls frame rate
}

void checkSleepMode() {
  // Ensure we use the correct sensitivity for wake-up/sleep checks
  useShakeSensitivity = false; // Use SLEEP_THRESHOLD when checking general activity/wake conditions
  bool motionDetectedByAccel = false;
  if (accelerometerEnabled && g_accelerometer_initialized) { // Only check accel if enabled and initialized
      motionDetectedByAccel = detectMotion(); // Read sensor and check against SLEEP_THRESHOLD
  }

  if (sleepModeActive) {
      // --- Currently Sleeping ---
      if (motionDetectedByAccel) {
          Serial.println("Motion detected while sleeping, waking up..."); // DEBUG
          wakeFromSleepMode(); // Call the wake function
          // wakeFromSleepMode already sets sleepModeActive = false and resets lastActivityTime
      }
      // No need to check timeout when already sleeping
  } else {
      // --- Currently Awake ---
      if (motionDetectedByAccel) {
          // Any motion resets the activity timer when awake
          lastActivityTime = millis();
          // Serial.println("Motion detected while awake, activity timer reset."); // DEBUG Optional
      } else {
          // No motion detected while awake, check timeout for sleep entry
          // Ensure sleepModeEnabled config flag is checked
          if (sleepModeEnabled && (millis() - lastActivityTime > SLEEP_TIMEOUT_MS)) {
              Serial.println("Inactivity timeout reached, entering sleep..."); // DEBUG
              enterSleepMode();
          }
      }
  }
}

void loop() {
  //unsigned long frameStartTimeMillis = millis(); // Timestamp at frame start

  // Check BLE connection status (low frequency check is fine)
  //bool isConnected = NimBLEDevice::getServer()->getConnectedCount() > 0;
  handleBLEStatusLED(); // Update status LED based on connection

  // --- Handle Inputs and State Updates ---

  // Check for motion and handle sleep/wake state
  // checkSleepMode(); // Handles motion detection, wake, and sleep entry

  // Only process inputs/updates if NOT in sleep mode
  if (!sleepModeActive) {
       // --- Motion Detection (for shake effect to change view) ---
        if (accelerometerEnabled && g_accelerometer_initialized) {
            useShakeSensitivity = true; // Use high threshold for shake detection
            if (detectMotion()) { // detectMotion uses the current useShakeSensitivity
                if (currentView != 9) { // Prevent re-triggering if already spiral
                    previousView = currentView; // Save the current view.
                    currentView = 9;            // Switch to spiral eyes view
                    spiralStartMillis = millis(); // Record the trigger time.
                    Serial.println("Shake detected! Switching to Spiral View."); // DEBUG
                    if (deviceConnected) {
                        xTaskNotifyGive(bleNotifyTaskHandle);
                    }
                    lastActivityTime = millis(); // Shake is activity
                }
            }
            useShakeSensitivity = false; // Switch back to low threshold for general sleep/wake checks
        }

        // --- Handle button inputs for view changes ---
        #if defined(ARDUINO_ADAFRUIT_MATRIXPORTAL_ESP32S3) || defined(BUTTONS_AVAILABLE) // Add define if you use external buttons
            static bool viewChangedByButton = false;

           if (debounceButton(BUTTON_UP)) {
                currentView = (currentView + 1);
                if (currentView >= totalViews) currentView = 0;
                viewChangedByButton = true;
                saveLastView(currentView);
                lastActivityTime = millis();
            }
            if (debounceButton(BUTTON_DOWN)) {
                currentView = (currentView - 1);
                 if (currentView < 0) currentView = totalViews - 1;
                viewChangedByButton = true;
                saveLastView(currentView);
                lastActivityTime = millis();
            }

            if (viewChangedByButton) {
                Serial.printf("View changed by button: %d\n", currentView); // DEBUG
                if (deviceConnected) {
                    xTaskNotifyGive(bleNotifyTaskHandle);
                }
                // Reset specific view states if necessary when changing views
                 if (currentView != 5) { // If leaving blush view
                     blushState = BLUSH_INACTIVE;
                     blushBrightness = 0;
                     // disableBlush(); // Clears pixels, but displayCurrentView will clear anyway
                 }
                 if (currentView != 9) { // Reset spiral timer
                    spiralStartMillis = 0;
                 }
                viewChangedByButton = false; // Reset flag
            }
        #endif


      // --- Proximity Sensor Logic: Blush Trigger AND Eye Bounce Trigger ---
        // static unsigned long lastSensorReadTime = 0; // Already global
        if (millis() - lastSensorReadTime >= sensorInterval) {
             lastSensorReadTime = millis();
             #if defined(APDS_AVAILABLE) // Ensure sensor is available
                
                    uint8_t proximity = apds.readProximity();
                    // Serial.printf("Proximity: %d\n", proximity); // DEBUG Proximity Value

                    bool bounceJustTriggered = false; // Flag to avoid double blush trigger

            // Eye Bounce Trigger - Switch to View 17 (Circle Eyes) (MODIFIED)
            if (proximity >= 15 && !isEyeBouncing) {
                Serial.println("Proximity! Starting eye bounce sequence & switching to Circle Eyes (View 17).");
                
                // Store current view and switch to Circle Eyes (view 17)
                if (currentView != 9 && currentView != 17) { // Avoid conflicting with spiral or re-triggering
                    viewBeforeEyeBounce = currentView; // <<< MODIFIED: Store current view here
                    
                    currentView = 17; // Switch to "Circle Eyes" view
                    // saveLastView(currentView); // Optional: save temporary view 17
                    if (deviceConnected) {
                        xTaskNotifyGive(bleNotifyTaskHandle);
                    }
                } else if (currentView == 17) {
                    // Already in Circle Eyes view, just re-trigger bounce
                }

                isEyeBouncing = true;
                eyeBounceStartTime = millis();
                eyeBounceCount = 0;
                lastActivityTime = millis();
                bounceJustTriggered = true; // Mark that bounce (and view switch) happened

                // Also trigger blush effect to happen ON view 17 (Circle Eyes)
                if (blushState == BLUSH_INACTIVE) { // Only trigger if not already blushing
                    Serial.println("Proximity! Also triggering blush effect on Circle Eyes.");
                    blushState = BLUSH_FADE_IN;
                    blushStateStartTime = millis();
                    wasBlushOverlay = false; // Blush on view 17 is part of that view's temporary effect
                    originalViewBeforeBlush = viewBeforeEyeBounce; // Blush on view 17 uses context of viewBeforeEyeBounce
                }
            }

                    if (proximity >= 15 && blushState == BLUSH_INACTIVE && !bounceJustTriggered) {
                // Trigger blush if:
                // 1. Proximity detected
                // 2. Not already blushing
                // 3. Bounce wasn't *just* triggered (which might have handled its own blush)
                // 4. Current view is NOT the dedicated blush view (5)
                //    AND current view is NOT the temporary bounce/circle view (17)
                if (currentView != 5 && currentView != 17) {
                     Serial.println("Proximity! Triggering blush overlay effect.");
                     blushState = BLUSH_FADE_IN;
                     blushStateStartTime = millis();
                     lastActivityTime = millis();
                     
                     // This is a blush overlay on the current stable view
                     originalViewBeforeBlush = currentView; // Store the view that is getting the overlay
                     wasBlushOverlay = true;                // Mark that this blush is an overlay
                }
            }
        }
     #endif

        // --- Update Adaptive Brightness ---
        if (autoBrightnessEnabled) {
            maybeUpdateBrightness();
        }
        // Manual brightness is applied immediately on BLE write if autoBrightness is off.

        // --- Update Animation States ---
        updateBlinkAnimation(); // Update blink animation once per loop
        updateEyeBounceAnimation(); // NEW: Update eye bounce animation progress
        updateIdleHoverAnimation();   // NEW: Update idle eye hover animation progress
        
        if (blushState != BLUSH_INACTIVE) {
             updateBlush();
        }

        // --- Revert from Spiral View Timer ---
        if (currentView == 9 && spiralStartMillis > 0 && (millis() - spiralStartMillis >= 5000)) {
             Serial.println("Spiral timeout, reverting view."); // DEBUG
             currentView = previousView;
             spiralStartMillis = 0;
             if (deviceConnected) {
                xTaskNotifyGive(bleNotifyTaskHandle);
             }
        }

        // --- Update Temperature Sensor Periodically ---
        static unsigned long lastTempUpdateLocal = 0; // Renamed to avoid conflict
        const unsigned long tempUpdateIntervalLocal = 5000; // 5 seconds
        if (millis() - lastTempUpdateLocal >= tempUpdateIntervalLocal) {
            maybeUpdateTemperature();
            lastTempUpdateLocal = millis();
        }

  } // End of (!sleepModeActive) block

// --- Display Rendering ---
updateMouthMovement(); // Update mouth movement if needed
//displayCurrentView(currentView); // Draw the appropriate view based on state

// --- Check sleep again AFTER processing inputs ---
// This allows inputs like button presses or BLE commands to reset the activity timer *before* checking for sleep timeout
checkSleepMode();

// Check for changes in brightness
//checkBrightness();

// --- Frame Rate Calculation ---
calculateFPS(); // Update FPS counter

vTaskDelay(pdMS_TO_TICKS(5)); // yield to the display & BLE tasks

/*
// If the current view is one of the plasma views, increase the interval to reduce load
if (currentView == 2 || currentView == 10) {
  baseInterval = 10; // Use 15 ms for plasma view
}
*/
}

