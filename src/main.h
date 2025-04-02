#ifndef MAIN_H
#define MAIN_H

#include <FastLED.h>
#include "userPreferences.h"
#include "deviceConfig.h"
#include "ble.h"

#include "xtensa/core-macros.h"
#include "bitmaps.h"
#include "customFonts/lequahyper20pt7b.h" // Stylized font
#include <Fonts/FreeSansBold18pt7b.h>     // Larger font
#define BAUD_RATE 115200                  // serial debug port baud rate

////////////////////// DEBUG MODE //////////////////////
#define DEBUG_MODE 1 // Set to 1 to enable debug outputs

#if DEBUG_MODE
#define DEBUG_BLE
#define DEBUG_VIEWS
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
  return (t >= 1.0f) ? 1.0f : t * t;
}

float easeOutQuad(float t)
{
  return (t <= 0.0f) ? 0.0f : 1.0f - (1.0f - t) * (1.0f - t);
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

int16_t textX;   // Current text position (X)
int16_t textY;   // Current text position (Y)
int16_t textMin; // Text pos. (X) when scrolled off left edge
char txt[64];    // Buffer to hold scrolling message text
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

uint8_t userBrightness = getUserBrightness(); // e.g., default 204 (80%)

// Map userBrightness (1-255) to hwBrightness (1-255).
// int hwBrightness = map(userBrightness, 1, 100, 1, 255);

// Map userBrightness (1-255) to sliderBrightness (1-100);
int sliderBrightness = map(userBrightness, 1, 255, 1, 100);

// Convert the userBrightness into a scale factor (0.0 to 1.0)
// Here, we simply divide userBrightness by 255.0 to get a proportion.
float globalBrightnessScale = userBrightness / 255.0;

unsigned long lastAmbientUpdateTime = 0;
const unsigned long ambientUpdateInterval = 500; // update every 500 milliseconds
uint8_t adaptiveBrightness = userBrightness;     // current brightness used by the system
uint8_t targetBrightness = userBrightness;       // target brightness for adaptive adjustment

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
  //apds.enableProximityInterrupt();
}

// Setup functions for adaptive brightness & proximity sensing using APDS9960:
uint8_t getAdaptiveBrightness()
{
  unsigned long now = millis();
  if (now - lastAmbientUpdateTime >= ambientUpdateInterval)
  {
    lastAmbientUpdateTime = now;
    if (apds.colorDataReady())
    {
      uint16_t r, g, b, c;
      apds.getColorData(&r, &g, &b, &c);
      Serial.print("Ambient light level (clear channel): ");
      Serial.println(c);
      targetBrightness = map(c, 0, 1023, 10, 255);
    }
    // else: sensor not ready—keep last computed adaptiveBrightness.
    // If the sensor isn't ready, targetBrightness remains unchanged.
  }

  // Smoothly transition adaptiveBrightness towards targetBrightness.
  // Here, we use a smoothing factor; adjust the factor (0 < smoothingFactor <= 1)
  // to control the speed of transition.
  const float smoothingFactor = 0.05; // Adjust this value to control the smoothing speed (0.0 to 1.0)
  adaptiveBrightness = (uint8_t)(adaptiveBrightness + (targetBrightness - adaptiveBrightness) * smoothingFactor);

  return adaptiveBrightness;
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

#endif /* MAIN_H */