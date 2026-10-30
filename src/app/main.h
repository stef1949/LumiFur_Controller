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
#include "app/Views.h"

#include "config/RuntimeSettings.h"
#include "app/Sensors.h"
#include "app/PowerManagement.h"
#include "core/Easing.h"

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
