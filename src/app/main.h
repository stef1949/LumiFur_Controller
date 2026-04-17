#ifndef MAIN_H
#define MAIN_H

#include <FastLED.h>
#include "config/debug_config.h"
#include "perf_tuning.h"
#include "config/userPreferences.h"
#include "hardware/deviceConfig.h"
#include "ble/ble.h"
#include "core/AdaptiveBrightness.h"
#include "SPI.h"
#include "xtensa/core-macros.h"
#include "assets/bitmaps.h"
#include <stdio.h>
#include "esp_ota_ops.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <esp_task_wdt.h>
#include <cmath>

// #include "assets/fonts/lequahyper20pt7b.h" // Stylized font
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
  VIEW_STROBE_EFFECT,
  VIEW_PIXEL_DUST,
  VIEW_STATIC_COLOR,
  VIEW_RAINBOW_GRADIENT,
  VIEW_RAINBOW_LINEAR_BAND,
  VIEW_ALT_FACE,
  VIEW_VIDEO_PLAYER,
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
#ifndef PERF_MONITORING
#define PERF_MONITORING LF_PERF_MONITORING_DEFAULT // Set to 1 to collect runtime timing/heap metrics
#endif
#ifndef PERF_LOGGING
#define PERF_LOGGING LF_PERF_LOGGING_DEFAULT // Set to 1 to emit periodic perf logs
#endif
#ifndef PERF_REPORT_INTERVAL_MS
#define PERF_REPORT_INTERVAL_MS 1000UL
#endif
#ifndef PERF_SELF_TEST
#define PERF_SELF_TEST 0
#endif
////////////////////////////////////////////////////////

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
  #if DEBUG_MODE
  Serial.println("CPU frequency reduced to 80MHz for power saving");
  #endif
}

void restoreNormalCPUSpeed()
{
  setCpuFrequencyMhz(240); // Set CPU frequency back to default (240MHz)
  #if DEBUG_MODE
  Serial.println("CPU frequency restored to 240MHz");
  #endif
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

// Preferences are initialized in setup(), so defer loading persisted brightness until then.
uint8_t userBrightness = 255;

int sliderBrightness = map(userBrightness, 1, 255, 1, 100);

// Convert the userBrightness into a scale factor (0.0 to 1.0)
// Here, dividing userBrightness by 255.0 to get a proportion.
extern float globalBrightnessScale;
extern uint16_t globalBrightnessScaleFixed;
void updateGlobalBrightnessScale(uint8_t brightness);

inline bool hasElapsedSince(unsigned long now, unsigned long last, unsigned long interval)
{
  return static_cast<unsigned long>(now - last) >= interval;
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
  #if DEBUG_MODE
  Serial.println("Applying configuration options...");
#endif
  if (autoBrightnessEnabled)
  {
    #if DEBUG_MODE
    Serial.println("Auto Brightness enabled: adjusting brightness automatically.");
    #endif
    // Example: call a function to update auto brightness (if implemented)
    // updateAutoBrightness();
    configApplyAutoBrightness = true;
  }
  else
  {
    #if DEBUG_MODE
    Serial.println("Auto brightness disabled. Applying user-set brightness.");
    #endif
    dma_display->setBrightness8(userBrightness);
    updateGlobalBrightnessScale(userBrightness);
    syncBrightnessState(userBrightness);
    #if DEBUG_MODE
    Serial.printf("Applied manual brightness: %u\n", userBrightness);
    #endif
  }

  if (accelerometerEnabled)
  {
    #if DEBUG_MODE
    Serial.println("Accelerometer enabled.");
    #endif
    // Insert code to enable accelerometer-driven actions here.
    configApplyAccelerometer = true;
  }
  else
  {
    #if DEBUG_MODE
    Serial.println("Accelerometer disabled.");
    #endif
    // Optionally disable or ignore accelerometer actions.
    configApplyAccelerometer = false;
  }

  if (sleepModeEnabled)
  {
    #if DEBUG_MODE
    Serial.println("Sleep mode enabled: device can enter sleep mode.");
    #endif
    // Update sleep-related thresholds or enable sleep mode triggers.
    configApplySleepMode = true;
  }
  else
  {
    #if DEBUG_MODE
    Serial.println("Sleep mode disabled: forcing device to remain awake.");
    #endif
    configApplySleepMode = false; // Ensure the device remains awake when sleep mode is disabled.
  }

  if (staticColorModeEnabled)
  {
    #if DEBUG_MODE
    Serial.println("Static color mode enabled: using user-selected color.");
    #endif
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
      #if DEBUG_MODE
      Serial.println("Aurora mode enabled: switching to aurora palette.");
      #endif
      // Assume auroraPalette and defaultPalette are defined globally.
      // currentPalette = auroraPalette;
      configApplyAuroraMode = true;
    }
    else
    {
      #if DEBUG_MODE
      Serial.println("Aurora mode disabled: using default palette.");
      #endif
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
const unsigned long dvdUpdateInterval = 40;

// First logo (left panel)
int dvdX1 = 0, dvdY1 = 0;
int dvdVX1 = 1, dvdVY1 = 1;
uint16_t dvdColor1;

// Second logo (right panel)
int dvdX2 = 64, dvdY2 = 0;
int dvdVX2 = 1, dvdVY2 = 1;
uint16_t dvdColor2;

#endif /* MAIN_H */
