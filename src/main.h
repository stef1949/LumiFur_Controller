#ifndef MAIN_H
#define MAIN_H

#include <FastLED.h>
#include "userPreferences.h"
#include "deviceConfig.h"
#include "ble.h"
#include "SPI.h"
#include "xtensa/core-macros.h"
#include "bitmaps.h"
#include <stdio.h>
#include "esp_ota_ops.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <esp_task_wdt.h>

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
  VIEW_PIXEL_DUST,

  // This special entry will automatically hold the total number of views.
  // It must always be the last item in the enum.
  TOTAL_VIEWS
};

// Global variables to store the accessory settings.
bool autoBrightnessEnabled = true;
bool accelerometerEnabled = true;
bool sleepModeEnabled = true;
bool auroraModeEnabled = true;
bool constantColorConfig = false;
CRGB constantColor = CRGB::Green; // Default color for constant color mode
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
#define DEBUG_MODE 1          // Set to 1 to enable debug outputs
#define DEBUG_MICROPHONE 0    // Set to 1 to enable microphone debug outputs
#define DEBUG_ACCELEROMETER 0 // Set to 1 to enable accelerometer debug outputs
#define DEBUG_BRIGHTNESS 0    // Set to 1 to enable brightness debug outputs
#if DEBUG_MODE
#define DEBUG_BLE
#define DEBUG_VIEWS
#endif
////////////////////////////////////////////////////////

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

/*
// Easing functions with bounds checking
float easeInQuad(float t)
{
  return (t >= 1.0f) ? 1.0f : t * t;
}
*/

// Easing functions with bounds checking
float easeInQuad(float t)
{
  if (t < 0.0f)
    return 0.0f;
  if (t > 1.0f)
    return 1.0f;
  return t * t;
}

/*
float easeOutQuad(float t)
{
  return (t <= 0.0f) ? 0.0f : 1.0f - (1.0f - t) * (1.0f - t);
}
*/

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
  const uint8_t delayTime = 5;

  if (millis() - lastUpdate < delayTime)
    return;
  lastUpdate = millis();

  brightness += step;
  if (brightness >= 255 || brightness <= 0)
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
CRGBPalette16 palettes[] = {ForestColors_p, LavaColors_p, HeatColors_p, RainbowColors_p, CloudColors_p};
CRGBPalette16 currentPalette = palettes[0];

CRGB ColorFromCurrentPalette(uint8_t index = 0, uint8_t brightness = 255, TBlendType blendType = LINEARBLEND)
{
  return ColorFromPalette(currentPalette, index, brightness, blendType);
}

// #include "EffectsLayer.hpp" // FastLED CRGB Pixel Buffer for which the patterns are drawn
// EffectsLayer effects(VPANEL_W, VPANEL_H);

// Helper functions for drawing text and graphics ----------------------------
void buffclear(CRGB *buf);
uint16_t XY16(uint16_t x, uint16_t y);
void mxfill(CRGB *leds);
uint16_t colorWheel(uint8_t pos);
void drawText(int colorWheelOffset);

// Power management scopes
void reduceCPUSpeed()
{
  // Set CPU frequency to lowest setting (80MHz vs 240MHz default)
  setCpuFrequencyMhz(80);
  Serial.println("CPU frequency reduced to 80MHz for power saving");
}

void restoreNormalCPUSpeed()
{
  // Set CPU frequency back to default (240MHz)
  setCpuFrequencyMhz(240);
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
int16_t textX;   // Current text position (X)
int16_t textY;   // Current text position (Y)
int16_t textMin; // Text pos. (X) when scrolled off left edge
extern char txt[64];    // Buffer to hold scrolling message text
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

// Map userBrightness (1-255) to hwBrightness (1-255).
// int hwBrightness = map(userBrightness, 1, 100, 1, 255);

// Map userBrightness (1-255) to sliderBrightness (1-100);
int sliderBrightness = map(userBrightness, 1, 255, 1, 100);

// Convert the userBrightness into a scale factor (0.0 to 1.0)
// Here, we simply divide userBrightness by 255.0 to get a proportion.
extern float globalBrightnessScale;
extern uint16_t globalBrightnessScaleFixed;
void updateGlobalBrightnessScale(uint8_t brightness);

unsigned long lastAmbientUpdateTime = 0;
const unsigned long ambientUpdateInterval = 500; // update every 500 milliseconds
uint8_t adaptiveBrightness = userBrightness;     // current brightness used by the system
uint8_t targetBrightness = userBrightness;       // target brightness for adaptive adjustment

// float ambientLux = 0.0;
static uint16_t lastKnownClearValue = 0; // Initialize to a sensible default
float smoothedLux = 50.0f;
const float luxSmoothingFactor = 0.15f;        // Adjust between 0.05 - 0.3
float currentBrightness = 128.0f;              // NEW: Current brightness as a float for smoothing
const float brightnessSmoothingFactor = 0.05f; // NEW: How quickly brightness adapts (0.01-0.1)
int lastBrightness = 0;
const int brightnessThreshold = 3; // Only update if change > X
unsigned long lastLuxUpdate = 0;
const unsigned long luxUpdateInterval = 100; // NEW: every 100 milliseconds (10Hz) - ADJUST THIS!

void setupAdaptiveBrightness()
{
  // Initialize APDS9960 proximity sensor if found
  if (!apds.begin())
  {
    Serial.println("failed to initialize proximity sensor! Please check your wiring.");
    while (1)
      ;
  }
  Serial.println("Proximity sensor initialized!");
  apds.enableProximity(true);                  // enable proximity mode
  apds.enableColor(true);                      // enable color mode
  apds.setProximityInterruptThreshold(0, 175); // set the interrupt threshold to fire when proximity reading goes above 175
  apds.enableProximityInterrupt();
}

// Setup functions for adaptive brightness & proximity sensing using APDS9960:
uint16_t getRawClearChannelValue()
{
  static unsigned long lastLogTime = 0;
  const unsigned long logInterval = 500; // Log 2 times per second

  if (apds.colorDataReady())
  {
    uint16_t r, g, b, c;
    apds.getColorData(&r, &g, &b, &c);

#if DEBUG_BRIGHTNESS
    Serial.print("Ambient light level (clear channel): ");
    lastKnownClearValue = c;
    Serial.println(c);
#else
    lastKnownClearValue = c; // Update last known good value
#endif
    return c; // Return the raw clear channel value
  }
  else
  {
    return lastKnownClearValue; // Return the last good value if current not available
  }
}

void updateAdaptiveBrightness()
{
  // If auto brightness is disabled, just apply the manual user brightness and exit.
  if (!autoBrightnessEnabled)
  {
    // Only set brightness if it has changed to avoid unnecessary calls
    if (lastBrightness != userBrightness)
    {
      dma_display->setBrightness8(userBrightness);
      lastBrightness = userBrightness;
#ifdef DEBUG_BRIGHTNESS
      Serial.printf(">>>> MANUAL: BRIGHTNESS SET TO %d <<<<\n", userBrightness);
#endif
    }
    return;
  }

  uint16_t rawClearValue = getRawClearChannelValue();

  float currentLuxEquivalent = static_cast<float>(rawClearValue);

  smoothedLux = (luxSmoothingFactor * currentLuxEquivalent) + ((1.0f - luxSmoothingFactor) * smoothedLux);

  // --- CRITICAL CALIBRATION SECTION ---

  const int min_brightness_output = 80;   // Fall back to user-set brightness when dark NOTE: use  userBrightness to revert to user setting
  const int max_brightness_output = 255;  // Preserve full-range capability
  const float min_clear_for_map = 2.0f;  // Adjusted dark threshold
  const float max_clear_for_map = 50.0f; // Adjusted bright threshold
                                    // OBSERVE rawClearValue via Serial.print to refine these.

   // ADJUST THESE VALUES:
  // For earlier dimming (dims in brighter conditions):
  // const float min_clear_for_map = 200.0f;           // Higher value = starts dimming earlier
  
  // For later dimming (only dims in very dark conditions):
  // const float min_clear_for_map = 20.0f;            // Lower value = only dims when very dark
  
  // For reaching full brightness sooner (in less bright conditions):
  // const float max_clear_for_map = 800.0f;           // Lower value = reaches max brightness sooner
  
  // For reaching full brightness later (only in very bright conditions):
  // const float max_clear_for_map = 2000.0f;          // Higher value = needs more light for full brightness
  
  // For different minimum brightness level:
  // const int min_brightness_output = 30;             // Fixed minimum instead of user brightness
  
  // For different maximum brightness level:
  // const int max_brightness_output = 200;            // Cap maximum brightness below 255

  // --- END CRITICAL CALIBRATION SECTION ---

  long targetBrightnessLong = map(static_cast<long>(smoothedLux),
                                  static_cast<long>(min_clear_for_map),
                                  static_cast<long>(max_clear_for_map),
                                  min_brightness_output,
                                  max_brightness_output);
  int targetBrightnessCalc = constrain(static_cast<int>(targetBrightnessLong), min_brightness_output, max_brightness_output);

#if DEBUG_BRIGHTNESS
  Serial.printf("ADAPT: RawC=%u, SmoothC=%.1f, TargetBr=%d, LastBr=%d, Thr=%d\n",
                rawClearValue, smoothedLux, targetBrightnessCalc, lastBrightness, brightnessThreshold); // Corrected variable name
#endif
  if (abs(targetBrightnessCalc - lastBrightness) >= brightnessThreshold)
  {
    uint8_t currentBrightness = static_cast<uint8_t>(targetBrightnessCalc);
    dma_display->setBrightness8(currentBrightness);
    updateGlobalBrightnessScale(currentBrightness);
    lastBrightness = targetBrightnessCalc; // Update lastBrightness ONLY when a display change is made
#if DEBUG_BRIGHTNESS
    Serial.printf(">>>> ADAPT: BRIGHTNESS SET TO %d <<<<\n", targetBrightnessCalc);
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
  // Always call updateAdaptiveBrightness to handle both manual and auto modes.
  // The function itself will decide what to do based on autoBrightnessEnabled.
  if (millis() - lastLuxUpdate >= luxUpdateInterval)
  {
    updateAdaptiveBrightness();
    lastLuxUpdate = millis();
  }
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

  if (constantColorConfig)
  {
    Serial.println("Constant color mode enabled.");
    uint8_t r = 255, g = 255, b = 255; // Example default values for red, green, and blue
    uint16_t rgb565 = dma_display->color565(constantColor.r, constantColor.g, constantColor.b);
    dma_display->fillScreen(rgb565);
    dma_display->flipDMABuffer();
    bool configApplyConstantColor = true;
  }
  else
  {
    Serial.println("Constant color mode disabled.");
    // Reset the display to the previous state or palette.
    bool configApplyConstantColor = false;
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
