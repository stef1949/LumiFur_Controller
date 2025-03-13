#ifdef IDF_BUILD
#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/gpio.h>
#include "sdkconfig.h"
#endif

#define DEBUG_BLE
#define DEBUG_VIEWS

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include "xtensa/core-macros.h"
#include "bitmaps.h"
#include "customFonts/lequahyper20pt7b.h"       // Stylized font
#include <Fonts/FreeSansBold18pt7b.h>           // Larger font
//#include <Wire.h>                             // For I2C sensors
#ifdef VIRTUAL_PANE
#include <ESP32-VirtualMatrixPanel-I2S-DMA.h>
#else
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#endif


#include "main.h"

//BLE Libraries
#include <NimBLEDevice.h>

// BLE UUIDs
#define SERVICE_UUID                    "01931c44-3867-7740-9867-c822cb7df308"
#define CHARACTERISTIC_UUID             "01931c44-3867-7427-96ab-8d7ac0ae09fe"
#define CONFIG_CHARACTERISTIC_UUID      "01931c44-3867-7427-96ab-8d7ac0ae09ff"
#define TEMPERATURE_CHARACTERISTIC_UUID "01931c44-3867-7b5d-9774-18350e3e27db"
//#define ULTRASOUND_CHARACTERISTIC_UUID  "01931c44-3867-7b5d-9732-12460e3a35db"

//#define DESC_USER_DESC_UUID  0x2901  // User Description descriptor
//#define DESC_FORMAT_UUID     0x2904  // Presentation Format descriptor


//#include "EffectsLayer.hpp" // FastLED CRGB Pixel Buffer for which the patterns are drawn
//EffectsLayer effects(VPANEL_W, VPANEL_H);

// ----------------------------------------------------------------
// ----------------------------------------------------------------
// ------------------- LumiFur Global Variables -------------------
// ----------------------------------------------------------------
// ----------------------------------------------------------------

//Brightness control
// Retrieve the brightness value from preferences
int userBrightness = getUserBrightness(); // e.g., default 204 (80%)
// Map userBrightness (1-100) to hardware brightness (1-255).
int hwBrightness = map(userBrightness, 1, 100, 1, 255);
// Convert the userBrightness into a scale factor (0.0 to 1.0)
// Here, we simply divide by 100.0 to get a proportion.
float globalBrightnessScale = userBrightness / 100.0;

// View switching
uint8_t currentView = 4; // Current & initial view being displayed
const int totalViews = 11; // Total number of views to cycle through
//int userBrightness = 20; // Default brightness level (0-255)

unsigned long spiralStartTime = 0; // Global variable to record when spiral view started
int previousView = 0;              // Global variable to store the view before spiral

//Maw switching
int currentMaw = 1; // Current & initial maw being displayed
const int totalMaws = 2; // Total number of maws to cycle through
unsigned long mawChangeTime = 0; // Time when maw was changed
bool mawTemporaryChange = false; // Whether the maw is temporarily changed

//Loading Bar
int loadingProgress = 0; // Current loading progress (0-100)
int loadingMax = 100;    // Maximum loading value

//Blinking effect variables
unsigned long lastEyeBlinkTime = 0; // Last time the eyes blinked
unsigned long nextBlinkDelay = 1000; // Random delay between blinks
int blinkProgress = 0;              // Progress of the blink (0-100%)
bool isBlinking = false;            // Whether a blink is in progress

int blinkDuration = 300;            // Initial time for a full blink (milliseconds)
const int minBlinkDuration = 100;   // Minimum time for a full blink (ms)
const int maxBlinkDuration = 500;   // Maximum time for a full blink (ms)
const int minBlinkDelay = 1000;     // Minimum time between blinks (ms)
const int maxBlinkDelay = 5000;     // Maximum time between blinks (ms)

// Global constants for each sensitivity level
const float SLEEP_THRESHOLD = 2.0;  // for sleep mode detection
const float SHAKE_THRESHOLD = 40.0;  // for shake detection
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
const unsigned long fadeInDuration = 2000;  // Duration for fade-in (2 seconds)
const unsigned long fullDuration = 6000;      // Full brightness time after fade-in (6 seconds)
// Total time from trigger to start fade-out is fadeInDuration + fullDuration = 8 seconds.
const unsigned long fadeOutDuration = 2000;   // Duration for fade-out (2 seconds)
bool isBlushFadingIn = false;    // Flag to indicate we are in the fade‑in phase

// Blush brightness variable (0-255)
uint8_t blushBrightness = 0;

// Non-blocking sensor read interval
unsigned long lastSensorReadTime = 0;
const unsigned long sensorInterval = 50;  // sensor read every 50 ms


// Variables for plasma effect
uint16_t time_counter = 0, cycles = 0, fps = 0;
unsigned long fps_timer;

CRGB currentColor;
CRGBPalette16 palettes[] = {ForestColors_p, LavaColors_p, HeatColors_p, RainbowColors_p, CloudColors_p};
CRGBPalette16 currentPalette = palettes[0];

CRGB ColorFromCurrentPalette(uint8_t index = 0, uint8_t brightness = 255, TBlendType blendType = LINEARBLEND) {
  return ColorFromPalette(currentPalette, index, brightness, blendType);
}

// Button config --------------------------------------------------------------
bool debounceButton(int pin) {
  static uint32_t lastPressTime = 0;
  uint32_t currentTime = millis();
  
  if (digitalRead(pin) == LOW && (currentTime - lastPressTime) > 200) {
    lastPressTime = currentTime;
    return true;
  }
  return false;
}

// Sleep mode variables
// Define both timeout values and select the appropriate one in setup()
const unsigned long SLEEP_TIMEOUT_MS_DEBUG = 60000;    // 15 seconds (15,000 ms)
const unsigned long SLEEP_TIMEOUT_MS_NORMAL = 300000; // 5 minutes (300,000 ms)
unsigned long SLEEP_TIMEOUT_MS; // Will be set in setup() based on debugMode
bool sleepModeActive = false;
unsigned long lastActivityTime = 0;
float prevAccelX = 0, prevAccelY = 0, prevAccelZ = 0;
uint8_t preSleepView = 4;  // Store the view before sleep
uint8_t sleepBrightness = 15; // Brightness level during sleep (0-255)
uint8_t normalBrightness = userBrightness; // Normal brightness level
unsigned long sleepFrameInterval = 11; // Frame interval in ms (will be changed during sleep)

// Accelerometer object declaration
Adafruit_LIS3DH accel;

// Power management scopes
void reduceCPUSpeed() {
  // Set CPU frequency to lowest setting (80MHz vs 240MHz default)
  setCpuFrequencyMhz(80);
  Serial.println("CPU frequency reduced to 80MHz for power saving");
}

void restoreNormalCPUSpeed() {
  // Set CPU frequency back to default (240MHz)
  setCpuFrequencyMhz(240);
  Serial.println("CPU frequency restored to 240MHz");
}

void displayCurrentView(int view);


void wakeFromSleepMode() {
  if (!sleepModeActive) return; // Already awake
  
  Serial.println("Waking from sleep mode");
  sleepModeActive = false;
  currentView = preSleepView;  // Restore previous view
  
  // Restore normal CPU speed
  restoreNormalCPUSpeed();
  
  // Restore normal frame rate
  sleepFrameInterval = 11; // Back to ~90 FPS
  
  // Restore normal brightness
  dma_display->setBrightness8(normalBrightness);
  
  lastActivityTime = millis(); // Reset activity timer
  
  // Notify all clients if connected
  if (deviceConnected) {
    // Also send current view back to the app
    uint8_t viewValue = static_cast<uint8_t>(currentView);
    pFaceCharacteristic->setValue(&viewValue, 1);
    pFaceCharacteristic->notify();
    
    // Send a temperature update
    updateTemperature();
  }
  
  // Restore normal BLE advertising if not connected
  if (!deviceConnected) {
    NimBLEDevice::getAdvertising()->setMinInterval(160); // 100 ms (default)
    NimBLEDevice::getAdvertising()->setMaxInterval(240); // 150 ms (default)
    NimBLEDevice::startAdvertising();
  }
}

// Class to handle characteristic callbacks
class CharacteristicCallbacks : public NimBLECharacteristicCallbacks {
  void onRead(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) override {
        uint8_t viewValue = static_cast<uint8_t>(currentView);
        pCharacteristic->setValue(&viewValue, 1);
        Serial.printf("Read request - returned view: %d\n", viewValue);
    }

  void onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) {
        std::string value = pCharacteristic->getValue();
        if(value.length() > 0) {
            uint8_t newView = value[0];

          // Command to wake from sleep mode (e.g., sending 255)
          if (newView == 255 && sleepModeActive) {
            sleepModeActive = false;
            wakeFromSleepMode();
            return;
          }
          // Normal view change
          if (newView >= 1 && newView <=12 && newView != currentView) {
              currentView = newView;
              lastActivityTime = millis(); // BLE command counts as activity
              Serial.printf("Write request - new view: %d\n", currentView);
              pCharacteristic->notify();
            }
        }
    }

  void onStatus(NimBLECharacteristic* pCharacteristic, int code) override {
        Serial.printf("Notification/Indication return code: %d, %s\n", code, NimBLEUtils::returnCodeToString(code));
    }
  void onSubscribe(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo, uint16_t subValue) override {
        std::string str  = "Client ID: ";
        str             += connInfo.getConnHandle();
        str             += " Address: ";
        str             += connInfo.getAddress().toString();
        if (subValue == 0) {
            str += " Unsubscribed to ";
        } else if (subValue == 1) {
            str += " Subscribed to notifications for ";
        } else if (subValue == 2) {
            str += " Subscribed to indications for ";
        } else if (subValue == 3) {
            str += " Subscribed to notifications and indications for ";
        }
        str += std::string(pCharacteristic->getUUID());

        Serial.printf("%s\n", str.c_str());
    }
} chrCallbacks; 

class DescriptorCallbacks : public NimBLEDescriptorCallbacks {
    void onWrite(NimBLEDescriptor* pDescriptor, NimBLEConnInfo& connInfo) override {
        std::string dscVal = pDescriptor->getValue();
        Serial.printf("Descriptor written value: %s\n", dscVal.c_str());
    }

    void onRead(NimBLEDescriptor* pDescriptor, NimBLEConnInfo& connInfo) override {
        Serial.printf("%s Descriptor read\n", pDescriptor->getUUID().toString().c_str());
    }
} dscCallbacks;


//non-blocking LED status functions (Neopixel)
void fadeInAndOutLED(uint8_t r, uint8_t g, uint8_t b) {
    static int brightness = 0;
    static int step = 5;
    static unsigned long lastUpdate = 0;
    const uint8_t delayTime = 5;

    if (millis() - lastUpdate < delayTime) return;
    lastUpdate = millis();

    brightness += step;
    if (brightness >= 255 || brightness <= 0) step *= -1;
    
    statusPixel.setPixelColor(0, 
        (r * brightness) / 255,
        (g * brightness) / 255,
        (b * brightness) / 255);
    statusPixel.show();
}

void handleBLEConnection() {
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
    
    if (!deviceConnected) {
        fadeInAndOutLED(0, 0, 100); // Blue fade when disconnected
    }
}

void fadeBlueLED() {
    fadeInAndOutLED(0, 0, 100); // Blue color
}

// Bitmap Drawing Functions ------------------------------------------------
void drawXbm565(int x, int y, int width, int height, const char *xbm, uint16_t color = 0xffff) {
    // Ensure width is padded to the nearest byte boundary
    int byteWidth = (width + 7) / 8;
    
    // Pre-check if entire bitmap is out of bounds
    if (x >= dma_display->width() || y >= dma_display->height() || 
        x + width <= 0 || y + height <= 0) {
        return; // Completely out of bounds, nothing to draw
    }
    
    // Calculate visible region to avoid per-pixel boundary checks
    int startX = max(0, -x);
    int startY = max(0, -y);
    int endX = min(width, dma_display->width() - x);
    int endY = min(height, dma_display->height() - y);
    
    for (int j = startY; j < endY; j++) {
        uint8_t bitMask = 0x80 >> (startX & 7); // Start with the correct bit position
        int byteIndex = j * byteWidth + (startX >> 3); // Integer division by 8
        
        for (int i = startX; i < endX; i++) {
            // Check if the bit is set
            if (pgm_read_byte(&xbm[byteIndex]) & bitMask) {
                dma_display->drawPixel(x + i, y + j, color);
            }
            
            // Move to the next bit
            bitMask >>= 1;
            if (bitMask == 0) { // We've used all bits in this byte
                bitMask = 0x80; // Reset to the first bit of the next byte
                byteIndex++;     // Move to the next byte
            }
        }
    }
}

uint16_t plasmaSpeed = 1; // Lower = slower animation

void drawPlasmaXbm(int x, int y, int width, int height, const char *xbm, 
                 uint8_t time_offset = 0, float scale = 5.0) {
  int byteWidth = (width + 7) / 8;

  
  for (int j = 0; j < height; j++) {
    for (int i = 0; i < width; i++) {
      if (pgm_read_byte(&xbm[j * byteWidth + (i / 8)]) & (1 << (7 - (i % 8)))) {
        // Plasma calculation specific to pixel position
        uint8_t v = 
          sin8((x + i) * scale + time_counter) + 
          cos8((y + j) * scale + time_counter / 2) + 
          sin8((x + i + y + j) * scale * 0.5 + time_counter / 3);
        
        CRGB color = ColorFromPalette(currentPalette, v + time_offset);
        // Apply global brightness control from preferences
        color.r = (uint8_t)(color.r * globalBrightnessScale);
        color.g = (uint8_t)(color.g * globalBrightnessScale);
        color.b = (uint8_t)(color.b * globalBrightnessScale);

        uint16_t rgb565 = dma_display->color565(color.r, color.g, color.b);
        dma_display->drawPixel(x + i, y + j, rgb565);
      }
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
  //drawPlasmaXbm(0, 16, 64, 16, maw, 32, 2.0);
  //drawPlasmaXbm(64, 16, 64, 16, mawL, 32, 2.0);
  drawPlasmaXbm(0, 10, 64, 22, maw2Closed, 32, 0.5);
  drawPlasmaXbm(64, 10, 64, 22, maw2ClosedL, 32, 0.5);
}

void updatePlasmaFace() {
  static unsigned long lastUpdate = 0;
  const uint16_t frameDelay = 5; // Low delay for smoother transition
  
  if (millis() - lastUpdate > frameDelay) {
    lastUpdate = millis();
    time_counter += plasmaSpeed; // Speed of plasma animation
    
    // Cycle palette every 10 seconds
    if (millis() % 10000 < frameDelay) {
      currentPalette = palettes[random(0, sizeof(palettes)/sizeof(palettes[0]))];
    }
  }
}

//Array of all the bitmaps (Total bytes used to store images in PROGMEM = Cant remember)
// Create code to list available bitmaps for future bluetooth control

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
"slanteyes"
};

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
//spiral
};


// Sundry globals used for animation ---------------------------------------

int16_t  textX;        // Current text position (X)
int16_t  textY;        // Current text position (Y)
int16_t  textMin;      // Text pos. (X) when scrolled off left edge
char     txt[64];      // Buffer to hold scrolling message text
int16_t  ball[3][4] = {
  {  3,  0,  1,  1 },  // Initial X,Y pos+velocity of 3 bouncy balls
  { 17, 15,  1, -1 },
  { 27,  4, -1,  1 }
};
uint16_t ballcolor[3]; // Colors for bouncy balls (init in setup())


// SETUP - RUNS ONCE AT PROGRAM START --------------------------------------


// Error handler function. If something goes wrong, this is what runs.
void err(int x) {
  uint8_t i;
  pinMode(LED_BUILTIN, OUTPUT);       // Using onboard LED
  for(int i=1;;i++) {                     // Loop forever...
    digitalWrite(LED_BUILTIN, i & 1); // LED on/off blink to alert user
    delay(x);
  }
}

void displayLoadingBar() {
  dma_display->clearScreen();
  // Draw Apple Logos
  drawXbm565(23, 2, 18, 21, appleLogoApple_logo_black, dma_display->color565(255, 255, 255));
  drawXbm565(88, 2, 18, 21, appleLogoApple_logo_black, dma_display->color565(255, 255, 255));

  // Draw the loading bar background
  int barWidth = dma_display->width() - 80; // Width of the loading bar
  int barHeight = 5;                // Height of the loading bar
  int barX = (dma_display->width() / 4) - (barWidth / 2);        // Center X position
  int barY = (dma_display->height() - barHeight) / 2; // Center vertically
  //int barRadius = 4;                // Rounded corners
  dma_display->drawRect(barX, (barY * 2), barWidth, barHeight, dma_display->color565(9, 9, 9));
  dma_display->drawRect(barX + 64, (barY * 2), barWidth, barHeight, dma_display->color565(9, 9, 9));

  // Draw the loading progress
  int progressWidth = (barWidth - 2) * loadingProgress / loadingMax;
  dma_display->fillRect(barX + 1, (barY * 2) + 1, progressWidth, barHeight - 2, dma_display->color565(255, 255, 255));
  dma_display->fillRect((barX + 1) + 64, (barY * 2) + 1, progressWidth, barHeight - 2, dma_display->color565(255, 255, 255));

}

// Helper function for ease-in-out quadratic easing
float easeInOutQuad(float t) {
  return t < 0.5 ? 2 * t * t : -1 + (4 - 2 * t) * t;
}

// Easing functions with bounds checking
float easeInQuad(float t) {
  return (t >= 1.0f) ? 1.0f : t * t;
}

float easeOutQuad(float t) {
  return (t <= 0.0f) ? 0.0f : 1.0f - (1.0f - t) * (1.0f - t);
}

// Blink animation control with initialization
struct {
  unsigned long startTime = 0;
  unsigned long durationMs = 200;  // Default duration
  unsigned long closeDuration = 50;
  unsigned long holdDuration = 20;
  unsigned long openDuration = 50;
} blinkState;

void initBlinkState() {
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

  const unsigned long currentTime = millis();
  
  if (isBlinking) {
    const unsigned long elapsed = currentTime - blinkState.startTime;
    
    // Safety check for time overflow
    if (elapsed > blinkState.durationMs * 2) {
      isBlinking = false;
      return;
    }

    // Phase calculations with duration safeguards
    const unsigned long totalDuration = blinkState.closeDuration + 
                                      blinkState.holdDuration + 
                                      blinkState.openDuration;
    if (totalDuration == 0) {
      isBlinking = false;
      return;
    }

    if (elapsed < blinkState.closeDuration) {
      // Closing phase
      const float progress = (float)elapsed / blinkState.closeDuration;
      blinkProgress = 100 * easeInQuad(progress);
    } 
    else if (elapsed < blinkState.closeDuration + blinkState.holdDuration) {
      // Hold phase
      blinkProgress = 100;
    } 
    else if (elapsed < totalDuration) {
      // Opening phase
      const unsigned long openElapsed = elapsed - 
                                      (blinkState.closeDuration + 
                                       blinkState.holdDuration);
      const float progress = (float)openElapsed / blinkState.openDuration;
      blinkProgress = 100 * (1.0f - easeOutQuad(progress));
    } 
    else {
      // End blink
      isBlinking = false;
      blinkProgress = 0;
      lastEyeBlinkTime = currentTime;
      nextBlinkDelay = random(minBlinkDelay, maxBlinkDelay);
    }
  } 
  else {
    if (currentTime - lastEyeBlinkTime >= nextBlinkDelay) {
      // Initialize new blink with validated durations
      isBlinking = true;
      blinkState.startTime = currentTime;
      
      // Constrain durations to safe ranges
      blinkState.durationMs = constrain(random(minBlinkDuration, maxBlinkDuration), 
                                      50, 1000);
      
      // Calculate phases as percentages of total duration
      blinkState.closeDuration = blinkState.durationMs * random(25, 35) / 100;
      blinkState.holdDuration = blinkState.durationMs * random(5, 15) / 100;
      blinkState.openDuration = blinkState.durationMs - 
                              blinkState.closeDuration - 
                              blinkState.holdDuration;

      // Ensure minimum durations of 1 frame (16ms @60Hz)
      blinkState.closeDuration = max(blinkState.closeDuration, 16UL);
      blinkState.holdDuration = max(blinkState.holdDuration, 16UL);
      blinkState.openDuration = max(blinkState.openDuration, 16UL);
    }
  }
}

// Optimized rotation function
void drawRotatedBitmap(int16_t x, int16_t y, const uint8_t *bitmap, 
                      uint16_t w, uint16_t h, float cosA, float sinA, 
                      uint16_t color) {
  const int16_t cx = w/2, cy = h/2; // Center point
  
  for (int16_t j = 0; j < h; j++) {
    for (int16_t i = 0; i < w; i++) {
      if (pgm_read_byte(&bitmap[j * ((w+7)/8) + (i/8)]) & (1 << (7 - (i%8)))) {
        // Calculate rotated position
        int16_t dx = i - cx;
        int16_t dy = j - cy;
        int16_t newX = x + lround(dx * cosA - dy * sinA);
        int16_t newY = y + lround(dx * sinA + dy * cosA);
        
        // Boundary check and draw
        if (newX >= 0 && newX < PANE_WIDTH && newY >= 0 && newY < PANE_HEIGHT) {
          dma_display->drawPixel(newX, newY, color);
        }
      }
    }
  }
}

void updateRotatingSpiral() {
  static unsigned long lastUpdate = 0;
  static float currentAngle = 0;
  const unsigned long frameInterval = 11; // ~90 FPS (11ms per frame)
  unsigned long currentTime = millis();

  if (currentTime - lastUpdate < frameInterval) return;
  lastUpdate = currentTime;

  // Calculate color once per frame
  static uint8_t colorIndex = 0;
  colorIndex += 2; // Slower color transition
  CRGB currentColor = ColorFromPalette(currentPalette, colorIndex);
  uint16_t color = dma_display->color565(currentColor.r, currentColor.g, currentColor.b);
   
  // Clear the screen before drawing
  //dma_display->clearScreen();

  // Optimized rotation with pre-calculated values
  float radians = currentAngle * PI / 180.0;
  float cosA = cos(radians);
  float sinA = sin(radians);
  currentAngle = fmod(currentAngle + 6, 360); // Slower rotation (6° per frame)
  
  //Clear previous spiral frame
  //dma_display->fillRect(24, 15, 32, 25, 0);
  //dma_display->fillRect(105, 15, 32, 25, 0);
  // Draw both spirals using pre-transformed coordinates
  drawRotatedBitmap(24, 15, spiral, 25, 25,  cosA, sinA, color);
  drawRotatedBitmap(105, 15, spiralL, 25, 25,  cosA, sinA, color);
}



// Draw the blinking eyes
void blinkingEyes() {

  //dma_display->clearScreen(); // Clear the display

  // Draw  eyes
  if (currentView == 4 || currentView == 5) {

    drawPlasmaXbm(0, 0, 32, 16, Eye, 0, 1.0);     // Right eye
    drawPlasmaXbm(96, 0, 32, 16, EyeL, 128, 1.0); // Left eye (phase offset)
  }
  else if (currentView == 6) {

    drawPlasmaXbm(0, 0, 32, 12, semicircleeyes, 0, 1.0);     // Right eye
    drawPlasmaXbm(96, 0, 32, 12, semicircleeyes, 128, 1.0); // Left eye (phase offset)
  }
  else if (currentView == 7) {

    drawPlasmaXbm(0, 0, 31, 15, x_eyes, 0, 1.0);     // Right eye
    drawPlasmaXbm(96, 0, 31, 15, x_eyes, 128, 1.0); // Left eye (phase offset)
  }

  else if (currentView == 8) {

    drawPlasmaXbm(0, 0, 24, 16, slanteyes, 0, 1.0);     // Right eye
    drawPlasmaXbm(104, 0, 24, 16, slanteyesL, 128, 1.0); // Left eye (phase offset)
  }
  else if (currentView == 9) {

  }

  if (isBlinking) {
    // Calculate the height of the black box
    int boxHeight = map(blinkProgress, 0, 100, 0, 20); // Adjust height of blink. DEFAULT: From 0 to 16 pixels

    // Draw black boxes over the eyes
    dma_display->fillRect(0, 0, 32, boxHeight, 0);     // Cover the right eye
    dma_display->fillRect(96, 0, 32, boxHeight, 0);    // Cover the left eye
  }
}

// Function to disable/clear the blush display when the effect is over
void disableBlush() {
  Serial.println("Blush disabled!");
  // Clear the blush area to ensure the effect is removed
  dma_display->fillRect(45, 1, 18, 13, 0); // Clear right blush area
  dma_display->fillRect(72, 1, 18, 13, 0); // Clear left blush area
}

// Update the blush effect state (non‑blocking)
void updateBlush() {
  unsigned long now = millis();
  unsigned long elapsed = now - blushStateStartTime;
  
  switch (blushState) {
    case BLUSH_FADE_IN:
      if (elapsed < fadeInDuration) {
        // Gradually increase brightness from 0 to 255
        blushBrightness = map(elapsed, 0, fadeInDuration, 0, 255);
      } else {
        blushBrightness = 255;
        blushState = BLUSH_FULL;
        blushStateStartTime = now;  // Restart timer for full brightness phase
      }
      break;
      
    case BLUSH_FULL:
      if (elapsed >= fullDuration) {
        // After 6 seconds at full brightness, start fading out
        blushState = BLUSH_FADE_OUT;
        blushStateStartTime = now;  // Restart timer for fade‑out
      }
      // Brightness remains at 255 during this phase.
      break;
      
    case BLUSH_FADE_OUT:
      if (elapsed < fadeOutDuration) {
        // Decrease brightness gradually from 255 to 0
        blushBrightness = map(elapsed, 0, fadeOutDuration, 255, 0);
      } else {
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
  //Serial.print("Blush brightness: ");
  //Serial.println(blushBrightness);

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
dma_display->clearScreen(); // Clear the display

  int stripeHeight = dma_display->height() / 5; // Height of each stripe

  // Define colors in RGB565 format
  uint16_t lightBlue = dma_display->color565(0, 102/2, 204/2); // Deeper light blue
  uint16_t pink = dma_display->color565(255, 20/2, 147/2);    // Deeper pink
  uint16_t white = dma_display->color565(255/2, 255/2, 255/2);   // White (unchanged)

  // Draw stripes
  dma_display->fillRect(0, 0, dma_display->width(), stripeHeight, lightBlue);       // Top light blue stripe
  dma_display->fillRect(0, stripeHeight, dma_display->width(), stripeHeight, pink); // Pink stripe
  dma_display->fillRect(0, stripeHeight * 2, dma_display->width(), stripeHeight, white); // Middle white stripe
  dma_display->fillRect(0, stripeHeight * 3, dma_display->width(), stripeHeight, pink);  // Pink stripe
  dma_display->fillRect(0, stripeHeight * 4, dma_display->width(), stripeHeight, lightBlue); // Bottom light blue stripe
}

void protoFaceTest() {

   blinkingEyes();

   drawPlasmaXbm(0, 10, 64, 22, maw2Closed, 0, 1.0);     // Right eye
   drawPlasmaXbm(64, 10, 64, 22, maw2ClosedL, 128, 1.0); // Left eye (phase offset)

   
  if (currentView > 3) { // Only draw blush effect for face views, not utility views
    drawBlush();
  }

   drawPlasmaXbm(56, 10, 8, 8, nose, 64, 2.0);
   drawPlasmaXbm(64, 10, 8, 8, noseL, 64, 2.0);

}

void patternPlasma() {
  dma_display->clearScreen();
  for (int x = 0; x < dma_display->width() * 1; x++) {
    for (int y = 0; y < dma_display->height(); y++) {
      int16_t v = 128;
      uint8_t wibble = sin8(time_counter);
      v += sin16(x * wibble * 3 + time_counter);
      v += cos16(y * (128 - wibble) + time_counter);
      v += sin16(y * x * cos8(-time_counter) / 8);

      currentColor = ColorFromPalette(currentPalette, (v >> 8));
      uint16_t color = dma_display->color565(currentColor.r, currentColor.g, currentColor.b);
      dma_display->drawPixel(x, y, color);
    }
  }

  ++time_counter;
  ++cycles;
  ++fps;

  if (cycles >= 1024) {
    time_counter = 0;
    cycles = 0;
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
  static float breathingDirection = 1;  // 1 for increasing, -1 for decreasing
  
  // Update breathing effect
  if (millis() - lastBreathTime >= 50) {
    lastBreathTime = millis();
    
    // Update breathing brightness
    brightness += breathingDirection * 0.01;  // Slow breathing effect
    
    // Reverse direction at limits
    if (brightness >= 1.0) {
      brightness = 1.0;
      breathingDirection = -1;
    } else if (brightness <= 0.1) {  // Keep a minimum brightness
      brightness = 0.1;
      breathingDirection = 1;
    }
    
    // Apply breathing effect to overall brightness
    uint8_t currentBrightness = sleepBrightness * brightness;
    dma_display->setBrightness8(currentBrightness);
  }
  
  dma_display->clearScreen();
  
  // Simple animation for floating Zs
  if (millis() - lastAnimTime > 100) {
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
  if (millis() - lastBlinkTime > 4000) {  // Occasional slow blink
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
  drawXbm565(0, 20, 64, 8, maw2Closed, dma_display->color565(120, 120, 120));
  drawXbm565(64, 20, 64, 8, maw2ClosedL, dma_display->color565(120, 120, 120));
  
  dma_display->flipDMABuffer();
}

void setup() {

  Serial.begin(BAUD_RATE);
  delay(1000); // Delay for serial monitor to start
  Serial.println(" ");
  Serial.println("*****************************************************");
  Serial.println("*****************************************************");
  Serial.println("*****************************************************");
  Serial.println("*******************    LumiFur    *******************");
  Serial.println("*****************************************************");
  Serial.println("*****************************************************");
  Serial.println("*****************************************************");
  
  // Initialize Preferences
  initPreferences();
  
  // Retrieve and print stored brightness value.
  int userBrightness = getUserBrightness();
  // Map userBrightness (1-100) to hardware brightness (1-255).
  int hwBrightness = map(userBrightness, 1, 100, 1, 255);
  Serial.printf("Stored brightness: %d\n", userBrightness);
 

  Serial.println("Initializing BLE...");
  NimBLEDevice::init("LumiFur_Controller");
  NimBLEDevice::setPower(ESP_PWR_LVL_P9); // Power level 9 (highest) for best range
  NimBLEDevice::setSecurityAuth(BLE_SM_PAIR_AUTHREQ_BOND | BLE_SM_PAIR_AUTHREQ_SC);
  pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(&serverCallbacks);

  NimBLEService* pService = pServer->createService(SERVICE_UUID);
  
  // Face control characteristic with encryption
  pFaceCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID,
    //NIMBLE_PROPERTY::READ |
    //NIMBLE_PROPERTY::WRITE |
    NIMBLE_PROPERTY::NOTIFY |
    NIMBLE_PROPERTY::READ_ENC | // only allow reading if paired / encrypted
    NIMBLE_PROPERTY::WRITE_ENC  // only allow writing if paired / encrypted
  );
  
  // Set initial view value
  uint8_t viewValue = static_cast<uint8_t>(currentView);
  pFaceCharacteristic->setValue(&viewValue, 1);
  pFaceCharacteristic->setCallbacks(&chrCallbacks);

  // Temperature characteristic with encryption
  pTemperatureCharacteristic = 
    pService->createCharacteristic(
      TEMPERATURE_CHARACTERISTIC_UUID,
      NIMBLE_PROPERTY::READ |
      NIMBLE_PROPERTY::NOTIFY 
      //NIMBLE_PROPERTY::READ_ENC
  );
  
  // Config characteristic with encryption
  pConfigCharacteristic = pService->createCharacteristic(
      CONFIG_CHARACTERISTIC_UUID,
      NIMBLE_PROPERTY::NOTIFY |
      NIMBLE_PROPERTY::READ_ENC |
      NIMBLE_PROPERTY::WRITE_ENC
  );

  // Set up descriptors
  NimBLE2904* pFormat2904 = pFaceCharacteristic->create2904();
  pFormat2904->setFormat(NimBLE2904::FORMAT_UINT8);
  pFormat2904->setUnit(0x2700); // Unit-less number
  pFormat2904->setCallbacks(&dscCallbacks);

  // Add user description descriptor
  NimBLEDescriptor* pDesc = 
      pFaceCharacteristic->createDescriptor(
          "2901",
          NIMBLE_PROPERTY::READ,
          20
      );
  pDesc->setValue("Face Control");

  //nimBLEService* pBaadService = pServer->createService("BAAD");
  pService->start();

  /** Create an advertising instance and add the services to the advertised data */
  NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->setName("LumiFur_Controller");
  pAdvertising->addServiceUUID(pService->getUUID());
  pAdvertising->enableScanResponse(true);
  pAdvertising->start();

  Serial.println("BLE setup complete - advertising started");

  
  //Redefine pins if required
  //HUB75_I2S_CFG::i2s_pins _pins={R1, G1, BL1, R2, G2, BL2, CH_A, CH_B, CH_C, CH_D, CH_E, LAT, OE, CLK};
  //HUB75_I2S_CFG mxconfig(PANEL_WIDTH, PANEL_HEIGHT, PANELS_NUMBER);
  HUB75_I2S_CFG::i2s_pins _pins={R1_PIN, G1_PIN, B1_PIN, R2_PIN, G2_PIN, B2_PIN, A_PIN, B_PIN, C_PIN, D_PIN, E_PIN, LAT_PIN, OE_PIN, CLK_PIN};

  // Module configuration
  HUB75_I2S_CFG mxconfig(
    PANEL_WIDTH,   // module width
    PANEL_HEIGHT,   // module height
    PANELS_NUMBER,   // Chain length
    _pins          // Pin mapping
  );

  mxconfig.gpio.e = PIN_E;
  mxconfig.driver = HUB75_I2S_CFG::FM6126A;   // for panels using FM6126A chips
  mxconfig.clkphase = false;
  mxconfig.double_buff = true; // <------------- Turn on double buffer
#ifndef VIRTUAL_PANE
  dma_display = new MatrixPanel_I2S_DMA(mxconfig);
  dma_display->begin();
  dma_display->setBrightness8(hwBrightness);
#else
  chain = new MatrixPanel_I2S_DMA(mxconfig);
  chain->begin();
  chain->setBrightness8(hwBrightness);
  // create VirtualDisplay object based on our newly created dma_display object
  matrix = new VirtualMatrixPanel((*chain), NUM_ROWS, NUM_COLS, PANEL_WIDTH, PANEL_HEIGHT, CHAIN_TOP_LEFT_DOWN);
#endif

  dma_display->clearScreen();
  dma_display->flipDMABuffer();

  // Memory allocation for LED buffer
  ledbuff = (CRGB *)malloc(NUM_LEDS * sizeof(CRGB));
  if (ledbuff == nullptr) {
    Serial.println("Memory allocation for ledbuff failed!");
    err(250);  // Call error handler
  }

#if defined(ARDUINO_ADAFRUIT_MATRIXPORTAL_ESP32S3)
  // Initialize onboard components for MatrixPortal ESP32-S3
  statusPixel.begin(); // Initialize NeoPixel status light
  
  if (!accel.begin(0x19)) {  // Default I2C address for LIS3DH on MatrixPortal
    Serial.println("Could not find accelerometer, check wiring!");
    err(250);  // Fast blink = I2C error
  } else {
    Serial.println("Accelerometer found!");
    accel.setRange(LIS3DH_RANGE_4_G);  // 2G range is sufficient for motion detection
    //lis.setPerformanceMode(LIS3DH_MODE_HIGH_RESOLUTION);
    //lis.setDataRate(LIS3DH_DATARATE_100_HZ);
  }
#endif

  lastActivityTime = millis(); // Initialize the activity timer

  randomSeed(analogRead(0)); // Seed the random number generator
  //randomSeed(analogRead(0)); // Seed the random number generator
  
  // Set sleep timeout based on debug mode
  SLEEP_TIMEOUT_MS = debugMode ? SLEEP_TIMEOUT_MS_DEBUG : SLEEP_TIMEOUT_MS_NORMAL;

  // Set initial plasma color palette
  currentPalette = RainbowColors_p;
  // Set up buttons if present
  /* ----------------------------------------------------------------------
  Use internal pull-up resistor, as the up and down buttons 
  do not have any pull-up resistors connected to them 
  and pressing either of them pulls the input low.
  ------------------------------------------------------------------------- */
  #ifdef BUTTON_UP
    pinMode(BUTTON_UP, INPUT_PULLUP);
  #endif
  #ifdef BUTTON_DOWN
    pinMode(BUTTON_DOWN, INPUT_PULLUP);
  #endif

// Initialize APDS9960 proximity sensor
if(!apds.begin()){
  Serial.println("failed to initialize proximity sensor! Please check your wiring.");
  while(1);
}
 Serial.println("Proximity sensor initialized!");
  apds.enableProximity(true); //enable proximity mode
  apds.setProximityInterruptThreshold(0, 175); //set the interrupt threshold to fire when proximity reading goes above 175
  apds.enableProximityInterrupt(); //enable the proximity interrupt

}

uint8_t wheelval = 0;

void displayCurrentMaw() {
  switch (currentMaw) {
    case 0:
  //dma_display->clearScreen();
  Serial.println("Displaying Closed maw");
  drawXbm565(0, 10, 64, 22, maw2Closed, dma_display->color565(255, 255, 255));
  drawXbm565(64, 10, 64, 22, maw2ClosedL, dma_display->color565(255, 255, 255));
  //dma_display->flipDMABuffer();
  break;

  case 1:
  //dma_display->clearScreen();
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
  static unsigned long lastFrameTime = 0; // Track the last frame time
  const unsigned long frameInterval = 5; // Consistent 30 FPS
  static int previousView = -1; // Track the last active view

  // If we're in sleep mode, don't display the normal view
  if (sleepModeActive) {
    displaySleepMode();
    return;
  }
  
   if (millis() - lastFrameTime < frameInterval) return; // Skip frame if not time yet
  lastFrameTime = millis(); // Update last frame time

  dma_display->clearScreen(); // Clear the display

if (view != previousView) { // Check if the view has changed
    if (view == 5) {
      // Reset fade logic when entering the blush view
      blushStateStartTime = millis();
      isBlushFadingIn = true;
      blushState = BLUSH_FADE_IN;  // Start fade‑in state
      Serial.println("Entered blush view, resetting fade logic");
    }
    previousView = view; // Update the last active view
  }

  switch (view) {

  case 0: // Scrolling text Debug View
    // Every frame, we clear the background and draw everything anew.
    // This happens "in the background" with double buffering, that's
    // why you don't see everything flicker. It requires double the RAM,
    // so it's not practical for every situation.

    // Draw big scrolling text
    dma_display->setCursor(textX, textY);
    dma_display->print(txt);

    // Update text position for next frame. If text goes off the
    // left edge, reset its position to be off the right edge.
    if ((--textX) < textMin)
      textX = dma_display->width();

    // Draw the three bouncy balls on top of the text...
    for (byte i = 0; i < 3; i++)
    {
      // Draw 'ball'
      dma_display->fillCircle(ball[i][0], ball[i][1], 5, ballcolor[i]);
      // Update ball's X,Y position for next frame
      ball[i][0] += ball[i][2];
      ball[i][1] += ball[i][3];
      // Bounce off edges
      if ((ball[i][0] == 0) || (ball[i][0] == (dma_display->width() - 1)))
        ball[i][2] *= -1;
      if ((ball[i][1] == 0) || (ball[i][1] == (dma_display->height() - 1)))
        ball[i][3] *= -1;
    }
    
    break;

  case 1: // Loading bar effect
  //dma_display->clearScreen();
    displayLoadingBar();
    if (loadingProgress <= loadingMax)
    {
      displayLoadingBar(); // Update the loading bar
      loadingProgress++;   // Increment progress
      //delay(50);           // Adjust speed of loading animation
    } else {
      // Reset or transition to another view
      loadingProgress = 0;
    }
    //dma_display->flipDMABuffer();
    break;

  case 2: // Pattern plasma
    patternPlasma();
    //delay(10);
    //dma_display->flipDMABuffer();
    break;

  case 3:
    //dma_display->clearScreen(); // Clear the display
    drawTransFlag();
    //delay(20);
    //dma_display->flipDMABuffer();
    break;

  case 4: // Normal
    protoFaceTest();
    updatePlasmaFace();
    updateBlinkAnimation(); // Update blink animation progress
    //delay(20);              // Short delay for smoother animation
    break;

  case 5: // Blush with fade in effect
    protoFaceTest();
    updatePlasmaFace();
    updateBlinkAnimation(); // Update blink animation progress
    break;

  case 6: // Dialated pupils
    protoFaceTest();
    updatePlasmaFace();
    updateBlinkAnimation(); // Update blink animation progress
    break;

  case 7: // X eyes
    protoFaceTest();
    updatePlasmaFace();
    updateBlinkAnimation(); // Update blink animation progress
    break;

  case 8: // Slant eyes
    protoFaceTest();
    updatePlasmaFace();
    updateBlinkAnimation(); // Update blink animation progress
    break;

  case 9: // Spiral eyes
    protoFaceTest();
    updateRotatingSpiral();
//  updateBlinkAnimation(); // Update blink animation progress
    break;

  case 10: //Plasma bitmap test
    drawPlasmaFace();
    updatePlasmaFace();
    updateBlinkAnimation();
    break;
/*
  case 10: // UwU eyes
    protoFaceTest();
    updateBlinkAnimation(); // Update blink animation progress
    break;
  
*/
  default:
      // Optional: Handle unsupported views
      //dma_display->clearScreen();
      break;
  }

  // Only flip buffer if not spiral view (spiral handles its own flip)
  //if (view != 9) 

  if (debugMode) {
    // --- Begin FPS counter overlay ---
    static unsigned long lastFpsTime = 0;
    static int frameCount = 0;
    frameCount++;
    unsigned long currentTime = millis();
    if (currentTime - lastFpsTime >= 1000) {
        fps = frameCount;
        frameCount = 0;
        lastFpsTime = currentTime;
    }
    char fpsText[8];
    sprintf(fpsText, "FPS: %d", fps);
    dma_display->setTextColor(dma_display->color565(255, 255, 0)); // Yellow text
    // Position the counter at a corner (adjust as needed)
    dma_display->setCursor((dma_display->width() / 4) + 10 , 1);
    dma_display->print(fpsText);
    // --- End FPS counter overlay ---
  }

  dma_display->flipDMABuffer();
}

// Helper function to get the current threshold value
float getCurrentThreshold() {
  return useShakeSensitivity ? SHAKE_THRESHOLD : SLEEP_THRESHOLD;
}

// Add with your other utility functions
bool detectMotion() {
  //accel.read();
  
  sensors_event_t event;
  // Obtain a snapshot of the current accelerometer data.
  accel.getEvent(&event);


  //double x = accel.x / 1000.0;  // Convert to G
  //double y = accel.y / 1000.0;
  //double z = accel.z / 1000.0;
  
  // Calculate the movement delta from last reading
  float deltaX = fabs(event.acceleration.x - prevAccelX);
  float deltaY = fabs(event.acceleration.y - prevAccelY);
  float deltaZ = fabs(event.acceleration.z - prevAccelZ);
  
  // Store current values for next comparison
  prevAccelX = event.acceleration.x;
  prevAccelY = event.acceleration.y;
  prevAccelZ = event.acceleration.z;
  
  // Use the current threshold based on the context
  float threshold = getCurrentThreshold();

  //Serial.print(">TLP: ");
  Serial.print(">accel_x=:");
  Serial.println(event.acceleration.x);
  Serial.print(">, accel_y=:");
  Serial.println(event.acceleration.y);
  Serial.print(">, accel_z=:");
  Serial.println(event.acceleration.y);
  
  // Check if movement exceeds threshold
  if (deltaX > threshold || 
      deltaY > threshold || 
      deltaZ > threshold) {
    return true;
  }
  
  return false;
}

void enterSleepMode() {
  Serial.println("Entering sleep mode");
  sleepModeActive = true;
  preSleepView = currentView;  // Save current view
  
  // Lower display brightness
  dma_display->setBrightness8(sleepBrightness);
  
  // Reduce CPU speed for power saving
  reduceCPUSpeed();
  
  // Reduce the update rate during sleep
  sleepFrameInterval = 100; // 10 FPS during sleep to save power
  
  // Notify all clients of sleep mode if connected
  if (deviceConnected) {
    // Send a message through the temperature characteristic
    char sleepMsg[] = "Sleep Mode Active";
    pTemperatureCharacteristic->setValue(sleepMsg);
    pTemperatureCharacteristic->notify();
  }
  
  // Additional sleep actions
  if (!deviceConnected) {
    // Increase BLE advertising interval to save power during sleep
    NimBLEDevice::getAdvertising()->setMinInterval(1600); // 1000 ms (units of 0.625 ms)
    NimBLEDevice::getAdvertising()->setMaxInterval(2000); // 1250 ms (units of 0.625 ms)
    NimBLEDevice::startAdvertising();
  }
}

void checkSleepMode() {
  static unsigned long lastAccelCheck = 0;
  const unsigned long accelCheckInterval = 1000; // Check motion once per second to save power
  
  // Only check accelerometer periodically to save power
  if (millis() - lastAccelCheck >= accelCheckInterval) {
    lastAccelCheck = millis();
    
    // Check for motion
    if (detectMotion()) {
      lastActivityTime = millis();
      
      // If we were in sleep mode, wake up
      if (sleepModeActive) {
        wakeFromSleepMode();
      }
    } 
    else {
      // Check if it's time to enter sleep mode
      if (!sleepModeActive && (millis() - lastActivityTime > SLEEP_TIMEOUT_MS)) {
        enterSleepMode();
      }
    }
  }
}



void loop(void) {
  unsigned long now = millis();
  bool isConnected = NimBLEDevice::getServer()->getConnectedCount() > 0;
  
  // Always handle BLE and temperature updates first.
  handleBLEConnection();
  updateTemperature();

  // Check for sleep mode (only on MatrixPortal ESP32-S3).
  #if defined(ARDUINO_ADAFRUIT_MATRIXPORTAL_ESP32S3)
    checkSleepMode();
  #endif

  // If the device is in sleep mode, use low-sensitivity motion detection to wake it.
  if (sleepModeActive) {
    // Ensure low sensitivity (useShakeSensitivity = false).
    useShakeSensitivity = false;
    if (detectMotion()) {
      // Motion detected with the low threshold – wake up the device.
      wakeFromSleepMode();
    }
  }
  else {
    // Device is active—handle button input, sensor reading, blush effect,
    // and strong motion detection (shake) to trigger spiral eyes.

    // --- Handle button inputs for view changes ---
    static int lastView = -1;
    bool viewChanged = false;
    if (debounceButton(BUTTON_UP)) {
      currentView = (currentView + 1) % totalViews;
      viewChanged = true;
    }
    if (debounceButton(BUTTON_DOWN)) {
      currentView = (currentView - 1 + totalViews) % totalViews;
      viewChanged = true;
    }
    if (viewChanged && isConnected) {
      uint8_t viewValue = static_cast<uint8_t>(currentView);
      pFaceCharacteristic->setValue(&viewValue, 1);
      pFaceCharacteristic->notify();
    }

    // --- Sensor reading and blush trigger ---
    if (now - lastSensorReadTime >= sensorInterval) {
      lastSensorReadTime = now;
      uint8_t proximity = apds.readProximity();
      Serial.print(">Proximity:");
      Serial.println(proximity);
      //Serial.print("Proximity: ");
      //Serial.println(proximity);
      // Trigger blush if the proximity condition is met and no blush is active.
      if (proximity >= 15 && blushState == BLUSH_INACTIVE) {
        Serial.println("Blush triggered by proximity!");
        blushState = BLUSH_FADE_IN;
        blushStateStartTime = now;
      }
      lastView = currentView;  // Update the last active view.
    }
    if (blushState != BLUSH_INACTIVE) {
      updateBlush();
      drawBlush();
    }

    // --- Check for a strong shake to trigger spiral eyes ---
    // Temporarily set high sensitivity.
    useShakeSensitivity = true;
    if (detectMotion() && currentView != 9) {
      previousView = currentView;   // Save the current view.
      currentView = 9;              // Switch to spiral eyes view (assumed view 9).
      spiralStartTime = millis();   // Record the trigger time.
    }
    // Reset sensitivity back to the low (sleep mode) threshold.
    useShakeSensitivity = false;
    // If spiral eyes view is active, revert back after 5 seconds.
    if (currentView == 9 && (millis() - spiralStartTime >= 5000)) {
      currentView = previousView;   // Return to the previous view.
    }
  }
  
  // --- Frame rate controlled display updates ---
  static unsigned long lastFrameTime = 0;
  const unsigned long currentFrameInterval = sleepModeActive ? sleepFrameInterval : 5;
  if (millis() - lastFrameTime >= currentFrameInterval) {
    lastFrameTime = millis();
    if (sleepModeActive) {
      displaySleepMode();
    } else {
      displayCurrentView(currentView);
    }
  }
}

