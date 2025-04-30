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

#include "main.h"

// BLE Libraries
#include <NimBLEDevice.h>

#include <vector> //For chunking example

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

// Brightness control
//  Retrieve the brightness value from preferences


// --- Performance Tuning ---
// Target ~50-60 FPS. Adjust as needed based on view complexity.
const unsigned long targetFrameIntervalMillis = 5; // 1000ms / 200 FPS = 5ms per frame
// ---


// View switching
uint8_t currentView = 15;   // Current & initial view being displayed
const int totalViews = 15; // Total number of views to cycle through
// int userBrightness = 20; // Default brightness level (0-255)


// Plasma animation optimization
uint16_t plasmaFrameDelay = 15; // ms between plasma updates (was 10)
// Spiral animation optimization
unsigned long rotationFrameInterval = 5; // ms between spiral rotation updates (was 11)


unsigned long spiralStartMillis = 0; // Global variable to record when spiral view started
int previousView = 0;              // Global variable to store the view before spiral

// Maw switching
int currentMaw = 1;              // Current & initial maw being displayed
const int totalMaws = 2;         // Total number of maws to cycle through
unsigned long mawChangeTime = 0; // Time when maw was changed
bool mawTemporaryChange = false; // Whether the maw is temporarily changed

// Loading Bar
int loadingProgress = 0; // Current loading progress (0-100)
int loadingMax = 100;    // Maximum loading value

// Blinking effect variables
unsigned long lastEyeBlinkTime = 0;  // Last time the eyes blinked
unsigned long nextBlinkDelay = 1000; // Random delay between blinks
int blinkProgress = 0;               // Progress of the blink (0-100%)
bool isBlinking = false;             // Whether a blink is in progress

int blinkDuration = 300;          // Initial time for a full blink (milliseconds)
const int minBlinkDuration = 100; // Minimum time for a full blink (ms)
const int maxBlinkDuration = 500; // Maximum time for a full blink (ms)
const int minBlinkDelay = 1000;   // Minimum time between blinks (ms)
const int maxBlinkDelay = 5000;   // Maximum time between blinks (ms)

// Global constants for each sensitivity level
const float SLEEP_THRESHOLD = 2.0;  // for sleep mode detection
const float SHAKE_THRESHOLD = 20.0; // for shake detection
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
const unsigned long SLEEP_TIMEOUT_MS_DEBUG = 100000;  // 15 seconds (15,000 ms)
const unsigned long SLEEP_TIMEOUT_MS_NORMAL = 300000; // 5 minutes (300,000 ms)
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
  if (millis() - lastTempUpdate >= tempUpdateInterval) {
    updateTemperature();
    lastTempUpdate = millis();
  }}

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
      notifyPending = true;
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
    } else {
      Serial.println("Error: Config payload is not 4 bytes.");
    }
  }
};

// Reuse one instance to avoid heap fragmentation
static ConfigCallbacks configCallbacks;


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
    statusPixel.show();
    oldDeviceConnected = deviceConnected;
  }
  if (!deviceConnected){
    fadeInAndOutLED(0, 0, 100); // Blue fade when disconnected
  }
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
  int startX = max(0, -x);
  int startY = max(0, -y);
  int endX = min(width, dma_display->width() - x);
  int endY = min(height, dma_display->height() - y);

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

uint16_t plasmaSpeed = 5; // Lower = slower animation

void drawPlasmaXbm(int x, int y, int width, int height, const char *xbm,
                   uint8_t time_offset = 0, float scale = 5.0, float animSpeed = 0.2f)
{
    // Calculate byte width using bit-shift instead of division
    int byteWidth = (width + 7) >> 3;

    // Scale the time value for animation speed (preserving refresh rate)
    // Effective time is slowed down when animSpeed < 1.0, and sped up when > 1.0.
    float effectiveTimeFloat = time_counter * animSpeed;
    uint8_t t  = (uint8_t) effectiveTimeFloat;
    uint8_t t2 = (uint8_t)(effectiveTimeFloat / 2);
    uint8_t t3 = (uint8_t)(effectiveTimeFloat / 3);
    
    const float scaleHalf = scale * 0.5f;
    // Precompute the scaled starting x-coordinate for efficiency
    const float startX = x * scale;

    // Loop over each row of the XBM image
    for (int j = 0; j < height; j++)
    {
        int yj = y + j;
        float yScale = yj * scale;
        // Precompute the cos component for this row using t2
        uint8_t cos_val = cos8(yScale + t2);
        // Precompute a portion of the third sine term
        float tempSum = (x + yj) * scaleHalf;
        // Setup the starting x value for the sin calculation
        float x_val = startX;

        // Loop over each column
        for (int i = 0; i < width; i++)
        {
            // Use bitwise arithmetic for efficiency
            int byteIndex = i >> 3;         // same as i / 8
            int bitMask = 1 << (7 - (i & 7)); // same as (1 << (7 - (i % 8)))
            
            if (pgm_read_byte(xbm + j * byteWidth + byteIndex) & bitMask)
            {
                // Calculate plasma components with the scaled time value
                uint8_t sin_val = sin8(x_val + t);
                uint8_t sin_val2 = sin8(tempSum + i * scaleHalf + t3);
                uint8_t v = sin_val + cos_val + sin_val2;

                // Retrieve the color from the palette and apply a brightness scale
                CRGB color = ColorFromPalette(currentPalette, v + time_offset);
                color.r = (uint8_t)(color.r * globalBrightnessScale);
                color.g = (uint8_t)(color.g * globalBrightnessScale);
                color.b = (uint8_t)(color.b * globalBrightnessScale);

                // Convert to display format and draw the pixel
                uint16_t rgb565 = dma_display->color565(color.r, color.g, color.b);
                dma_display->drawPixel(x + i, yj, rgb565);
            }
            // Increment the scaled x value for the next column
            x_val += scale;
        }
    }
}

void drawPlasmaFace() {
  // Draw eyes with different plasma parameters
  drawPlasmaXbm(0, 0, 32, 16, Eye, 0, 1.0);     // Right eye
  drawPlasmaXbm(96, 0, 32, 16, EyeL, 128, 1.0); // Left eye (phase offset)
  // Nose with finer pattern
  drawPlasmaXbm(56, 10, 8, 8, nose, 64, 2.0);
  drawPlasmaXbm(64, 10, 8, 8, noseL, 64, 2.0);
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
  }

  if (now - lastPaletteChange > paletteInterval)
  {
    lastPaletteChange = now;
    currentPalette = palettes[random(0, sizeof(palettes) / sizeof(palettes[0]))];
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
  //  Draw Apple Logos
  drawXbm565(23, 2, 18, 21, appleLogoApple_logo_black, dma_display->color565(255, 255, 255));
  drawXbm565(88, 2, 18, 21, appleLogoApple_logo_black, dma_display->color565(255, 255, 255));

  // Draw the loading bar background
  int barWidth = dma_display->width() - 80;               // Width of the loading bar
  int barHeight = 5;                                      // Height of the loading bar
  int barX = (dma_display->width() / 4) - (barWidth / 2); // Center X position
  int barY = (barHeight - barHeight) / 2;     // Center vertically
  // int barRadius = 4;                // Rounded corners
  dma_display->drawRect(barX, (barY * 2), barWidth, barHeight, dma_display->color565(9, 9, 9));
  dma_display->drawRect(barX + 64, (barY * 2), barWidth, barHeight, dma_display->color565(9, 9, 9));

  // Draw the loading progress
  int progressWidth = (barWidth - 2) * loadingProgress / loadingMax;
  dma_display->fillRect(barX + 1, (barY * 2) + 1, progressWidth, barHeight - 2, dma_display->color565(255, 255, 255));
  dma_display->fillRect((barX + 1) + 64, (barY * 2) + 1, progressWidth, barHeight - 2, dma_display->color565(255, 255, 255));
}

// Blink animation control with initialization
struct blinkState
{
  unsigned long startTime = 0;
  unsigned long durationMs = 200; // Default duration
  unsigned long closeDuration = 50;
  unsigned long holdDuration = 20;
  unsigned long openDuration = 50;
} blinkState;

void initBlinkState()
{
  blinkState.startTime = 0;
  blinkState.durationMs = 200;
  blinkState.closeDuration = 50;
  blinkState.holdDuration = 20;
  blinkState.openDuration = 50;
}

void updateBlinkAnimation() {
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
          blinkProgress = 0;
          lastEyeBlinkTime = now;
          nextBlinkDelay = random(minBlinkDelay, maxBlinkDelay);
          return;
      }
      
      // Pre-calculate phase thresholds.
      const unsigned long phase1End = closeDur;             // End of closing phase.
      const unsigned long phase2End = closeDur + holdDur;     // End of hold phase.

      if (elapsed < phase1End) {
          // Closing phase with ease-in.
          const float progress = static_cast<float>(elapsed) / closeDur;
          blinkProgress = 100 * easeInQuad(progress);
      }
      else if (elapsed < phase2End) {
          // Hold phase – full blink.
          blinkProgress = 100;
      }
      else if (elapsed < totalDuration) {
          // Opening phase with ease-out.
          const unsigned long openElapsed = elapsed - phase2End;
          const float progress = static_cast<float>(openElapsed) / openDur;
          blinkProgress = 100 * (1.0f - easeOutQuad(progress));
      }
      else {
          // Blink cycle complete; reset variables.
          isBlinking = false;
          blinkProgress = 0;
          lastEyeBlinkTime = now;
          nextBlinkDelay = random(minBlinkDelay, maxBlinkDelay);
      }
  }
  else if (now - lastEyeBlinkTime >= nextBlinkDelay) {
      // Start a new blink cycle.
      isBlinking = true;
      blinkState.startTime = now;
      blinkState.durationMs = constrain(random(minBlinkDuration, maxBlinkDuration), 50, 1000);

      // Set phase durations as percentages of the overall blink duration.
      blinkState.closeDuration = blinkState.durationMs * random(25, 35) / 100;
      blinkState.holdDuration  = blinkState.durationMs * random(5, 15) / 100;
      blinkState.openDuration  = blinkState.durationMs - blinkState.closeDuration - blinkState.holdDuration;

      // Ensure each phase has a minimum duration (e.g. 16 ms for one frame at ~60 Hz).
      blinkState.closeDuration = max(blinkState.closeDuration, 16UL);
      blinkState.holdDuration  = max(blinkState.holdDuration, 16UL);
      blinkState.openDuration  = max(blinkState.openDuration, 16UL);
  }
}

// Optimized rotation function
void drawRotatedBitmap(int16_t x, int16_t y, const uint8_t *bitmap,
                       uint16_t w, uint16_t h, float cosA, float sinA,
                       uint16_t color)
{
  const int16_t cx = w / 2, cy = h / 2; // Center point

  for (int16_t j = 0; j < h; j++)
  {
    for (int16_t i = 0; i < w; i++)
    {
      if (pgm_read_byte(&bitmap[j * ((w + 7) / 8) + (i / 8)]) & (1 << (7 - (i % 8))))
      {
        // Calculate rotated position
        int16_t dx = i - cx;
        int16_t dy = j - cy;
        int16_t newX = x + lround(dx * cosA - dy * sinA);
        int16_t newY = y + lround(dx * sinA + dy * cosA);

        // Boundary check and draw
        if (newX >= 0 && newX < PANE_WIDTH && newY >= 0 && newY < PANE_HEIGHT)
        {
          dma_display->drawPixel(newX, newY, color);
        }
      }
    }
  }
}

void updateRotatingSpiral() {
  static unsigned long lastUpdate = 0;
  static float currentAngle = 0;
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
  // dma_display->clearScreen(); // Clear the display

  // Draw  eyes
  if (currentView == 4 || currentView == 5)
  {
    drawPlasmaXbm(0, 0, 32, 16, Eye, 0, 1.0);     // Right eye
    drawPlasmaXbm(96, 0, 32, 16, EyeL, 128, 1.0); // Left eye (phase offset)
  }
  else if (currentView == 6)
  {
    drawPlasmaXbm(0, 0, 32, 12, semicircleeyes, 0, 1.0);    // Right eye
    drawPlasmaXbm(96, 0, 32, 12, semicircleeyes, 128, 1.0); // Left eye (phase offset)
  }
  else if (currentView == 7)
  {
    drawPlasmaXbm(0, 0, 31, 15, x_eyes, 0, 1.0);    // Right eye
    drawPlasmaXbm(96, 0, 31, 15, x_eyes, 128, 1.0); // Left eye (phase offset)
  }
  else if (currentView == 8)
  {
    drawPlasmaXbm(0, 0, 24, 16, slanteyes, 0, 1.0);      // Right eye
    drawPlasmaXbm(104, 0, 24, 16, slanteyesL, 128, 1.0); // Left eye (phase offset)
  }
  else if (currentView == 9)
  {

  }
  
  if (isBlinking)
  {
    // Calculate the height of the black box
    int boxHeight = map(blinkProgress, 0, 100, 0, 16); // Maybe use 16 (original panel height?) instead of 20? Check eye bitmap height.
    // Let's use 16 as the max height to cover typical eye bitmaps
    boxHeight = map(blinkProgress, 0, 100, 0, 16);
    // Draw black boxes over the eyes - Only draw if height > 0
    if (boxHeight > 0) {
      // Determine the coordinates precisely based on where Eye and EyeL are drawn (0,0 and 96,0 ?)
      // Assume Eye is 32x16 drawn at (0,0) and EyeL is 32x16 drawn at (96,0)
      dma_display->fillRect(0, 0, 32, boxHeight, 0);  // Cover the right eye (coords match drawPlasmaXbm)
      dma_display->fillRect(96, 0, 32, boxHeight, 0); // Cover the left eye (coords match drawPlasmaXbm)
  }}
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
      // Gradually increase brightness from 0 to 255
      blushBrightness = map(elapsed, 0, fadeInDuration, 0, 255);
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
      // After 6 seconds at full brightness, start fading out
      blushState = BLUSH_FADE_OUT;
      blushStateStartTime = now; // Restart timer for fade‑out
    }
    // Brightness remains at 255 during this phase.
    break;

  case BLUSH_FADE_OUT:
    if (elapsed < fadeOutDuration)
    {
      // Decrease brightness gradually from 255 to 0
      blushBrightness = map(elapsed, 0, fadeOutDuration, 255, 0);
    }
    else
    {
      blushBrightness = 0;
      blushState = BLUSH_INACTIVE;
      disableBlush();
    }
    break;

  default:
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

  blinkingEyes();

  drawPlasmaXbm(0, 10, 64, 22, maw2Closed, 0, 1.0);     // Right eye
  drawPlasmaXbm(64, 10, 64, 22, maw2ClosedL, 128, 1.0); // Left eye (phase offset)

  if (currentView > 3)
  { // Only draw blush effect for face views, not utility views
    drawBlush();
  }
  drawPlasmaXbm(56, 10, 8, 8, nose, 64, 2.0);
  drawPlasmaXbm(64, 10, 8, 8, noseL, 64, 2.0);
}

void patternPlasma() {
  // Pre-calculate values that only depend on time_counter
  // These are constant for the entire frame draw
  uint8_t wibble = sin8(time_counter);
  // Pre-calculate the cosine term and the division by 8 (as shift)
  // Note: cos8 argument is uint8_t, so -time_counter wraps around correctly.
  uint8_t cos8_val_shifted = cos8((uint8_t)(-time_counter)) >> 3; // Calculate cos8(-t)/8 once
  uint16_t time_val = time_counter; // Use a local copy for calculations if needed
  uint16_t term2_factor = 128 - wibble; // Calculate 128 - sin8(t) once

  // Get display dimensions once
  int display_width = dma_display->width();
  int display_height = dma_display->height();

  // Outer loop for X
  for (int x = 0; x < display_width; x++) {
      // Pre-calculate terms dependent only on X and time
      uint16_t term1_base = x * wibble * 3 + time_val;
      // The y*x part needs to be inside the Y loop, but we can precalculate x * cos8_val_shifted
      uint16_t term3_x_factor = x * cos8_val_shifted;

      // Inner loop for Y
      for (int y = 0; y < display_height; y++) {
          // Calculate plasma value 'v'
          // Start with base offset
          int16_t v = 128;
          // Add terms using pre-calculated values where possible
          v += sin16(term1_base);                   // sin16(x*wibble*3 + t)
          v += cos16(y * term2_factor + time_val); // cos16(y*(128-wibble) + t)
          v += sin16(y * term3_x_factor);          // sin16(y * (x * cos8(-t) >> 3))

          // Get color from palette using the upper 8 bits of 'v'
          // The >> 8 effectively scales and wraps the 16-bit value to 8 bits for palette lookup
          currentColor = ColorFromPalette(currentPalette, (uint8_t)(v >> 8));

          // Draw the pixel using RGB565 format directly if possible
          // This is often faster than drawPixelRGB888 as it might avoid internal conversion.
          uint16_t color565 = dma_display->color565(currentColor.r, currentColor.g, currentColor.b);
          dma_display->drawPixel(x, y, color565);

          // Original drawing call (keep for reference or if color565 doesn't work as expected)
          // dma_display->drawPixelRGB888(x, y, currentColor.r, currentColor.g, currentColor.b);
      }
  }

  // Increment counters (outside the loops)
  ++time_counter;
  // ++cycles; // If 'cycles' is purely for palette change, handle it there
  // ++fps; // FPS should be calculated globally in the main loop, not here per-effect

  // Palette changing logic (moved cycle counting here)
  static uint16_t plasma_cycles = 0; // Use a static local counter for this effect
  plasma_cycles++;
  if (plasma_cycles >= 1024) {
      // time_counter = 0; // Resetting time_counter might make the animation jump? Optional.
      plasma_cycles = 0; // Reset cycle counter for this effect
      // Select next palette randomly
      currentPalette = palettes[random(0, sizeof(palettes) / sizeof(palettes[0]))];
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

  // Draw closed or slightly open eyes
  if (millis() - lastBlinkTime > 4000)
  { // Occasional slow blink
    eyesOpen = !eyesOpen;
    lastBlinkTime = millis();
  }

  // Draw nose
  drawXbm565(56, 10, 8, 8, nose, dma_display->color565(100, 100, 100));
  drawXbm565(64, 10, 8, 8, noseL, dma_display->color565(100, 100, 100));

  if (eyesOpen) {
    // Draw slightly open eyes - just a small slit
    dma_display->fillRect(5, 5, 20, 2, dma_display->color565(150, 150, 150));
    dma_display->fillRect(103, 5, 20, 2, dma_display->color565(150, 150, 150));
  } else {
    // Draw closed eyes
    dma_display->drawLine(5, 10, 40, 12, dma_display->color565(150, 150, 150));
    dma_display->drawLine(83, 10, 40, 12, dma_display->color565(150, 150, 150));
  }
  // Draw sleeping mouth (slight curve)
  drawXbm565(0, 20, 10, 8, maw2Closed, dma_display->color565(120, 120, 120));
  drawXbm565(64, 20, 10, 8, maw2ClosedL, dma_display->color565(120, 120, 120));

  dma_display->flipDMABuffer();
}
// Star structure for starfield animation
struct Star {
  int x;
  int y;
  int speed;
};

const int NUM_STARS = 50;
Star stars[NUM_STARS];

void initStarfield() {
  for (int i = 0; i < NUM_STARS; i++) {
      stars[i].x = random(0, dma_display->width());
      stars[i].y = random(0, dma_display->height());
      stars[i].speed = random(1, 4); // Stars move at different speeds
  }
}

void updateStarfield() {
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
  while (true) {
    if (notifyPending && deviceConnected) {
      uint8_t bleViewValue = static_cast<uint8_t>(currentView);
      pFaceCharacteristic->setValue(&bleViewValue, 1);
      pFaceCharacteristic->notify();
      pConfigCharacteristic->notify(); // Optional if config actually changed
      notifyPending = false;
    }
    vTaskDelay(pdMS_TO_TICKS(20));
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

#if DEBUG_MODE
  Serial.println("DEBUG MODE ENABLED");
#endif
  Serial.println(" ");

  initTempSensor();

  // Initialize Preferences
  initPreferences();

  // — LOAD LAST VIEW —
  currentView = getLastView();
  previousView = currentView;
  Serial.printf("Loaded last view: %d\n", currentView);

  // Retrieve and print stored brightness value.
  // int userBrightness = getUserBrightness();
  // Map userBrightness (1-100) to hardware brightness (1-255).
  // int hwBrightness = map(userBrightness, 1, 100, 1, 255);
  Serial.printf("Stored brightness: %d\n", userBrightness);

  ///////////////////// Setup BLE ////////////////////////
  Serial.println("Initializing BLE...");
  //NimBLEDevice::init("LumiFur_Controller");
  NimBLEDevice::init("LF-052618");
  //NimBLEDevice::setPower(ESP_PWR_LVL_P9); // Power level 9 (highest) for best range
  //NimBLEDevice::setPower(ESP_PWR_LVL_P21, NimBLETxPowerType::All); // Power level 21 (highest) for best range
  NimBLEDevice::setPower(ESP_PWR_LVL_P9, NimBLETxPowerType::All); // Power level 21 (highest) for best range
  NimBLEDevice::setSecurityAuth(BLE_SM_PAIR_AUTHREQ_BOND | BLE_SM_PAIR_AUTHREQ_SC);
  pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(&serverCallbacks);

  NimBLEService *pService = pServer->createService(SERVICE_UUID);

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
    NIMBLE_PROPERTY::NOTIFY
  );
  //pTemperatureLogsCharacteristic->setCallbacks(&chrCallbacks);
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

  // nimBLEService* pBaadService = pServer->createService("BAAD");
  pService->start();

  /** Create an advertising instance and add the services to the advertised data */
  NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->setName("LumiFur_Controller");
  //pAdvertising->setAppearance(0);         // Appearance value (0 is generic)
  pAdvertising->enableScanResponse(true);      // Enable scan response to include more data
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
#else
  chain = new MatrixPanel_I2S_DMA(mxconfig);
  chain->begin();
  chain->setBrightness8(userBrightness);
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
  }
  else
  {
    Serial.println("Accelerometer found!");
    accel.setRange(LIS3DH_RANGE_2_G); // 4G range is sufficient for motion detection
    //accel.setDataRate(LIS3DH_DATARATE_100_HZ); // Set data rate to 100Hz
    //accel.setPerformanceMode(LIS3DH_MODE_LOW_POWER); // Set to low power mode
  }
#endif

  lastActivityTime = millis(); // Initialize the activity timer for sleep mode

  randomSeed(analogRead(0)); // Seed the random number generator

  // Set sleep timeout based on debug mode
  SLEEP_TIMEOUT_MS = DEBUG_MODE ? SLEEP_TIMEOUT_MS_DEBUG : SLEEP_TIMEOUT_MS_NORMAL;

  // Set initial plasma color palette
  currentPalette = RainbowColors_p;


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
  // Set up buttons if present
  /* ----------------------------------------------------------------------
  Use internal pull-up resistor, as the up and down buttons
  do not have any pull-up resistors connected to them
  and pressing either of them pulls the input low.
  ------------------------------------------------------------------------- */

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

  // Boot screen
  dma_display->clearScreen();
  dma_display->setTextSize(2); // Set text size
  dma_display->setCursor(0, 0);
  dma_display->setTextColor(dma_display->color565(255, 0, 255));
  dma_display->print("LumiFur"); // Set text size
  dma_display->setTextSize(1); // Set text size
  dma_display->setCursor(0, 20);
  dma_display->setTextColor(dma_display->color565(255, 255, 255));
  dma_display->print("Initializing..."); // Set text size
  dma_display->flipDMABuffer(); // Flip the buffer to show the boot screen
  delay(1500); // Delay to show boot screen

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

void displayCurrentMaw() {
  switch (currentMaw) {
  case 0:
    // dma_display->clearScreen();
    Serial.println("Displaying Closed maw");
    drawXbm565(0, 10, 64, 22, maw2Closed, dma_display->color565(255, 255, 255));
    drawXbm565(64, 10, 64, 22, maw2ClosedL, dma_display->color565(255, 255, 255));
    // dma_display->flipDMABuffer();
    break;

  case 1:
    // dma_display->clearScreen();
    Serial.println("Displaying open maw");
    drawXbm565(0, 10, 64, 22, maw2, dma_display->color565(255, 255, 255));
    drawXbm565(64, 10, 64, 22, maw2L, dma_display->color565(255, 255, 255));
    break;

  default:
    Serial.println("Unknown Maw State");
    break;
  }
}

void displayCurrentView(int view) {
  static int previousView = -1;           // Track the last active view

  // If we're in sleep mode, don't display the normal view
  if (sleepModeActive) {
    displaySleepMode();
    return;
  }

  if (view != previousView)
  { // Check if the view has changed
    if (view == 5)
    {
      // Reset fade logic when entering the blush view
      blushStateStartTime = millis();
      isBlushFadingIn = true;
      blushState = BLUSH_FADE_IN; // Start fade‑in state
      Serial.println("Entered blush view, resetting fade logic");
    }
    previousView = view; // Update the last active view
    dma_display->clearScreen(); // Clear the display only when switching views
  }

  switch (view) {

  case 0: // Scrolling text Debug View
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

  case 1: // Loading bar effect
    displayLoadingBar();
    loadingProgress++;
    if (loadingProgress > loadingMax)
    {
      loadingProgress = 0; // Reset smoothly
    }
    break;

  case 2: // Pattern plasma
    patternPlasma();
    //delay(10); // Short delay for smoother animation
    break;

  case 3:
    drawTransFlag();
    break;

  case 4: // Normal
    baseFace();
    updatePlasmaFace();
    updateBlinkAnimation(); // Update blink animation progress
    //delay(10);              // Short delay for smoother animation
    break;

  case 5: // Blush with fade in effect
    baseFace();
    updatePlasmaFace();
    updateBlinkAnimation(); // Update blink animation progress
    break;

  case 6: // Dialated pupils
    baseFace();
    updatePlasmaFace();
    updateBlinkAnimation(); // Update blink animation progress
    break;

  case 7: // X eyes
    baseFace();
    updatePlasmaFace();
    updateBlinkAnimation(); // Update blink animation progress
    break;

  case 8: // Slant eyes
    baseFace();
    updatePlasmaFace();
    updateBlinkAnimation(); // Update blink animation progress
    break;

  case 9: // Spiral eyes
    updatePlasmaFace();
    baseFace();
    updateRotatingSpiral();
    break;

  case 10: // Plasma bitmap test
    drawPlasmaFace();
    updatePlasmaFace();
    updateBlinkAnimation();
    break;

  case 11: // UwU eyes
    baseFace();
    updatePlasmaFace();
    updateBlinkAnimation(); // Update blink animation progress
    break;

  case 12: // Starfield
    updateStarfield();
    break;
  
  case 13: // BSOD
  dma_display->fillScreen(dma_display->color565(0, 0, 255)); // Blue screen
  dma_display->setTextColor(dma_display->color565(255, 255, 255)); // White text
  dma_display->setTextSize(2); // Set text size
  dma_display->setCursor(5, 5);
  dma_display->print(":(");
  dma_display->setCursor(69, 5);
  dma_display->print(":(");
    break;
  
  case 14: // DvD Logo
  updateDVDLogos();
    break;

  default:
    // Optional: Handle unsupported views
    // dma_display->clearScreen();
    break;
  }
  // Only flip buffer if not spiral view (spiral handles its own flip)
  // if (view != 9)


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
  sprintf(fpsText, "FPS: %d", fps);
  dma_display->setTextSize(1); // Set text size for FPS counter
  dma_display->setTextColor(dma_display->color565(255, 255, 255)); // White text
  // Position the counter at a corner (adjust as needed)
  dma_display->setCursor((dma_display->width() / 4) + 8, 1);
  dma_display->print(fpsText);
  // --- End FPS counter overlay ---


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
  accel.read();

  float x = accel.x / 1000.0; // Convert to G
  float y = accel.y / 1000.0;
  float z = accel.z / 1000.0;
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
            pTemperatureCharacteristic->notify(); // Consider notifyPending
       }
  }
  // No need to change sleepFrameInterval here, main loop timing controls frame rate
}

void checkSleepMode() {
  // Ensure we use the correct sensitivity for wake-up/sleep checks
  useShakeSensitivity = false; // Use SLEEP_THRESHOLD when checking general activity/wake conditions
  bool motionDetected = detectMotion(); // Read sensor and check against SLEEP_THRESHOLD

  if (sleepModeActive) {
      // --- Currently Sleeping ---
      if (motionDetected) {
          Serial.println("Motion detected while sleeping, waking up..."); // DEBUG
          wakeFromSleepMode(); // Call the wake function
          // wakeFromSleepMode already sets sleepModeActive = false and resets lastActivityTime
      }
      // No need to check timeout when already sleeping
  } else {
      // --- Currently Awake ---
      if (motionDetected) {
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
  unsigned long frameStartTimeMillis = millis(); // Timestamp at frame start

  // Check BLE connection status (low frequency check is fine)
  bool isConnected = NimBLEDevice::getServer()->getConnectedCount() > 0;
  handleBLEStatusLED(); // Update status LED based on connection

  // --- Handle Inputs and State Updates ---

  // Check for motion and handle sleep/wake state
  // checkSleepMode(); // Handles motion detection, wake, and sleep entry

  // Only process inputs/updates if NOT in sleep mode
  if (!sleepModeActive) {
        // --- Motion Detection (for shake effect) ---
        useShakeSensitivity = true; // Use high threshold for shake detection
        #if defined(ARDUINO_ADAFRUIT_MATRIXPORTAL_ESP32S3) || defined(ACCEL_AVAILABLE)
        if (detectMotion() && currentView != 9 && accelerometerEnabled) { // checkMotion also updates lastActivityTime
            if (currentView != 9) { // Prevent re-triggering if already spiral
                previousView = currentView; // Save the current view.
                currentView = 9;            // Switch to spiral eyes view
                spiralStartMillis = millis(); // Record the trigger time.
                 //Serial.println("Shake detected! Switching to Spiral View."); // DEBUG ONLY
                 if (isConnected) notifyPending = true; // Notify BLE clients of view change
            }
        }
        #endif
        useShakeSensitivity = false; // Switch back to low threshold for sleep checks

        // --- Handle button inputs for view changes ---
        #if defined(ARDUINO_ADAFRUIT_MATRIXPORTAL_ESP32S3) || defined(BUTTONS_AVAILABLE) // Add define if you use external buttons
            static bool viewChangedByButton = false;
            if (debounceButton(BUTTON_UP)) {
                currentView = (currentView + 1);
                if (currentView >= totalViews) currentView = 0; // Wrap around using totalViews
                viewChangedByButton = true;
                saveLastView(currentView);               // Persist the new view
                lastActivityTime = millis(); // Button press is activity
            }
            if (debounceButton(BUTTON_DOWN)) {
                currentView = (currentView - 1);
                 if (currentView < 0) currentView = totalViews - 1; // Wrap around using totalViews
                viewChangedByButton = true;
                saveLastView(currentView); // Persist the new view
                lastActivityTime = millis(); // Button press is activity
            }

            if (viewChangedByButton && isConnected) {
                // uint8_t bleViewValue = static_cast<uint8_t>(currentView); // Done by notify task
                // pFaceCharacteristic->setValue(&bleViewValue, 1);
                notifyPending = true; // Set the flag to notify via BLE task
            }
            if (viewChangedByButton) {
                 //Serial.printf("View changed by button: %d\n", currentView); // DEBUG ONLY
                // Reset specific view states if necessary when changing views
                 if (currentView != 5) { // If leaving blush view reset state
                     blushState = BLUSH_INACTIVE;
                     blushBrightness = 0;
                     // Optionally clear blush area immediately: disableBlush();
                 }
                 if (currentView != 9) { // Reset spiral timer if not in spiral view
                    spiralStartMillis = 0;
                 }
                viewChangedByButton = false; // Reset flag
            }
        #endif


        // --- Proximity Sensor and Blush Trigger ---
        static unsigned long lastSensorReadTime = 0; // Static local variable
        if (/*configApplyProximity && */millis() - lastSensorReadTime >= sensorInterval) { // Check config flag
             lastSensorReadTime = millis();
             #if defined(APDS_AVAILABLE) // Add define if you have the sensor
                uint8_t proximity = apds.readProximity();
                if (proximity >= 15 && blushState == BLUSH_INACTIVE && currentView != 5 ) { // Only trigger if not already blushing/in blush view
                    //Serial.println("Blush triggered by proximity!"); // DEBUG ONLY
                    // Triggering blush often involves changing view, handle that explicitly?
                    // Option 1: Just start the effect in the current view
                     blushState = BLUSH_FADE_IN;
                    // currentView = 5; // Assuming 5 is blush view
                    // blushState = BLUSH_FADE_IN; // State machine handles fade from here
                    blushStateStartTime = millis();
                    // if (isConnected) notifyPending = true;
                    lastActivityTime = millis(); // Proximity counts as activity
                }
             #endif
        }


        // --- Update Adaptive Brightness ---
        if (autoBrightnessEnabled) { // Check if auto brightness is enabled
            static unsigned long lastBrightnessUpdateTime = 0;
            if (millis() - lastBrightnessUpdateTime >= 1000) {
                lastBrightnessUpdateTime = millis();
                uint8_t adaptiveBrightness = getCurrentBirghtness(); // Assumes this exists and works
                dma_display->setBrightness8(adaptiveBrightness);
                 // Serial.printf("Adaptive Brightness: %d\n", adaptiveBrightness); // DEBUG ONLY
            }
        } else {
            // Use manual brightness if auto is off
             int hwBrightness = map(userBrightness, 1, 100, 1, 255);
            dma_display->setBrightness8(hwBrightness);
        }

        // --- Update Blush Effect State Machine ---
        if (blushState != BLUSH_INACTIVE) {
             updateBlush(); // Update state (fade in/full/fade out)
            // Drawing happens in baseFace or explicitly in displayCurrentView if needed
        }

        // --- Revert from Spiral View Timer ---
        if (currentView == 9 && spiralStartMillis > 0 && (millis() - spiralStartMillis >= 5000)) {
             //Serial.println("Spiral timeout, reverting view."); // DEBUG ONLY
             currentView = previousView; // Return to the previous view.
             spiralStartMillis = 0; // Reset timer
             if (isConnected) notifyPending = true;
        }

        // --- Update Temperature Sensor Periodically ---
        static unsigned long lastTempUpdate = 0; // Static local variable
        const unsigned long tempUpdateInterval = 5000; // 5 seconds
        if (millis() - lastTempUpdate >= tempUpdateInterval) {
            maybeUpdateTemperature(); // Handles reading and BLE notification if needed
            lastTempUpdate = millis();
        }

  } // End of (!sleepModeActive) block


   // --- Check sleep again AFTER processing inputs ---
   // This allows inputs like button presses or BLE commands to reset the activity timer *before* checking for sleep timeout
   checkSleepMode();

  // --- Display Rendering ---
  displayCurrentView(currentView); // Draw the appropriate view based on state

   // --- Frame Rate Calculation ---
   calculateFPS(); // Update FPS counter

   // --- Frame Rate Limiting ---
   unsigned long frameEndTimeMillis = millis();
   unsigned long frameDurationMillis = frameEndTimeMillis - frameStartTimeMillis;
   if (frameDurationMillis < targetFrameIntervalMillis) {
     // Yield remaining time to other tasks (like BLE stack, FreeRTOS scheduler)
     vTaskDelay(pdMS_TO_TICKS(targetFrameIntervalMillis - frameDurationMillis));
   } else {
       // Frame took too long, yield briefly anyway to prevent starving other tasks
       vTaskDelay(pdMS_TO_TICKS(1));
       // Serial.printf("Frame took too long: %lu ms\n", frameDurationMillis); // DEBUG ONLY
   }
 
  /*
  // If the current view is one of the plasma views, increase the interval to reduce load
  if (currentView == 2 || currentView == 10) {
    baseInterval = 10; // Use 15 ms for plasma view
  }
  */
  //delay(1); // Small delay to prevent overloading the CPU
}
