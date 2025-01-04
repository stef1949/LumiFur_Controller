/* ----------------------------------------------------------------------
Double-buffering (smooth animation) Protomatter library example.
PLEASE SEE THE "simple" EXAMPLE FOR AN INTRODUCTORY SKETCH.
Comments here pare down many of the basics and focus on the new concepts.

This example is written for a 64x32 matrix but can be adapted to others.
------------------------------------------------------------------------- */

#include <Adafruit_Protomatter.h>
#include "fonts/lequahyper20pt7b.h"       // Stylized font
#include <fonts/FreeSansBold18pt7b.h>     // Larger font
#include <FastLED.h>                      // For pattern plasma colors
#include <Wire.h>                       // For I2C sensors
/* ----------------------------------------------------------------------
The RGB matrix must be wired to VERY SPECIFIC pins, different for each
microcontroller board. This first section sets that up for a number of
supported boards.
------------------------------------------------------------------------- */

#if defined(_VARIANT_MATRIXPORTAL_M4_) // MatrixPortal M4
  uint8_t rgbPins[]  = {7, 8, 9, 10, 11, 12};
  uint8_t addrPins[] = {17, 18, 19, 20, 21};
  uint8_t clockPin   = 14;
  uint8_t latchPin   = 15;
  uint8_t oePin      = 16;
  #define BUTTON_UP 2
  #define BUTTON_DOWN 3
#elif defined(ARDUINO_ADAFRUIT_MATRIXPORTAL_ESP32S3) // MatrixPortal ESP32-S3
  uint8_t rgbPins[]  = {42, 41, 40, 38, 39, 37};
  uint8_t addrPins[] = {45, 36, 48, 35, 21};
  uint8_t clockPin   = 2;
  uint8_t latchPin   = 47;
  uint8_t oePin      = 14;
  #define BUTTON_UP 6
  #define BUTTON_DOWN 7
  #include <Adafruit_LIS3DH.h>      // For accelerometer
  #include <Adafruit_Sensor.h>      // For accelerometer
  #include <SPI.h>                  // For accelerometer
#elif defined(_VARIANT_FEATHER_M4_) // Feather M4 + RGB Matrix FeatherWing
  uint8_t rgbPins[]  = {6, 5, 9, 11, 10, 12};
  uint8_t addrPins[] = {A5, A4, A3, A2};
  uint8_t clockPin   = 13;
  uint8_t latchPin   = 0;
  uint8_t oePin      = 1;
#elif defined(__SAMD51__) // M4 Metro Variants (Express, AirLift)
  uint8_t rgbPins[]  = {6, 5, 9, 11, 10, 12};
  uint8_t addrPins[] = {A5, A4, A3, A2};
  uint8_t clockPin   = 13;
  uint8_t latchPin   = 0;
  uint8_t oePin      = 1;
#elif defined(_SAMD21_) // Feather M0 variants
  uint8_t rgbPins[]  = {6, 7, 10, 11, 12, 13};
  uint8_t addrPins[] = {0, 1, 2, 3};
  uint8_t clockPin   = SDA;
  uint8_t latchPin   = 4;
  uint8_t oePin      = 5;
#elif defined(NRF52_SERIES) // Special nRF52840 FeatherWing pinout
  uint8_t rgbPins[]  = {6, A5, A1, A0, A4, 11};
  uint8_t addrPins[] = {10, 5, 13, 9};
  uint8_t clockPin   = 12;
  uint8_t latchPin   = PIN_SERIAL1_RX;
  uint8_t oePin      = PIN_SERIAL1_TX;
#elif USB_VID == 0x239A && USB_PID == 0x8113 // Feather ESP32-S3 No PSRAM
  // M0/M4/RP2040 Matrix FeatherWing compatible:
  uint8_t rgbPins[]  = {6, 5, 9, 11, 10, 12};
  uint8_t addrPins[] = {A5, A4, A3, A2};
  uint8_t clockPin   = 13; // Must be on same port as rgbPins
  uint8_t latchPin   = RX;
  uint8_t oePin      = TX;
#elif USB_VID == 0x239A && USB_PID == 0x80EB // Feather ESP32-S2
  // M0/M4/RP2040 Matrix FeatherWing compatible:
  uint8_t rgbPins[]  = {6, 5, 9, 11, 10, 12};
  uint8_t addrPins[] = {A5, A4, A3, A2};
  uint8_t clockPin   = 13; // Must be on same port as rgbPins
  uint8_t latchPin   = RX;
  uint8_t oePin      = TX;
#elif defined(ESP32)
  // 'Safe' pins, not overlapping any peripherals:
  // GPIO.out: 4, 12, 13, 14, 15, 21, 27, GPIO.out1: 32, 33
  // Peripheral-overlapping pins, sorted from 'most expendible':
  // 16, 17 (RX, TX)
  // 25, 26 (A0, A1)
  // 18, 5, 9 (MOSI, SCK, MISO)
  // 22, 23 (SCL, SDA)
  uint8_t rgbPins[]  = {4, 12, 13, 14, 15, 21};
  uint8_t addrPins[] = {16, 17, 25, 26};
  uint8_t clockPin   = 27; // Must be on same port as rgbPins
  uint8_t latchPin   = 32;
  uint8_t oePin      = 33;
#elif defined(ARDUINO_TEENSY40)
  uint8_t rgbPins[]  = {15, 16, 17, 20, 21, 22}; // A1-A3, A6-A8, skip SDA,SCL
  uint8_t addrPins[] = {2, 3, 4, 5};
  uint8_t clockPin   = 23; // A9
  uint8_t latchPin   = 6;
  uint8_t oePin      = 9;
#elif defined(ARDUINO_TEENSY41)
  uint8_t rgbPins[]  = {26, 27, 38, 20, 21, 22}; // A12-14, A6-A8
  uint8_t addrPins[] = {2, 3, 4, 5};
  uint8_t clockPin   = 23; // A9
  uint8_t latchPin   = 6;
  uint8_t oePin      = 9;
#elif defined(ARDUINO_ADAFRUIT_FEATHER_RP2040)
  // RP2040 support requires the Earle Philhower board support package;
  // will not compile with the Arduino Mbed OS board package.
  // The following pinout works with the Adafruit Feather RP2040 and
  // original RGB Matrix FeatherWing (M0/M4/RP2040, not nRF version).
  // Pin numbers here are GP## numbers, which may be different than
  // the pins printed on some boards' top silkscreen.
  uint8_t rgbPins[]  = {8, 7, 9, 11, 10, 12};
  uint8_t addrPins[] = {25, 24, 29, 28};
  uint8_t clockPin   = 13;
  uint8_t latchPin   = 1;
  uint8_t oePin      = 0;
#endif

// Global Variables ---------------------------------------------------------

// Matrix measurements (Solely for shapes and text placement)
const int matrixWidth = 128;  // Width of the matrix in pixels
const int matrixHeight = 32; // Height of the matrix in pixels

// View switching
int currentView = 4; // Current & initial view being displayed
const int totalViews = 12; // Total number of views to cycle through

// Maw switching
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

//Blushing effect variables
unsigned long blushFadeStartTime = 0;
const unsigned long blushFadeDuration = 2000; // 2 seconds for full fade-in
bool isBlushFadingIn = true;                  // Whether the blush is currently fading in
uint8_t blushBrightness = 0;                  // Current blush brightness (0-255)

//Heartbeat effect variables
unsigned long lastHeartUpdate = 0;
int heartScale = 1;
bool isScalingUp = true; // Flag to control scaling direction

// Variables for plasma effect
uint16_t time_counter = 0, cycles = 0, fps = 0;
unsigned long fps_timer;

CRGB currentColor;
CRGBPalette16 palettes[] = {LavaColors_p, HeatColors_p, RainbowColors_p, RainbowStripeColors_p, CloudColors_p};
CRGBPalette16 currentPalette = palettes[0];

CRGB ColorFromCurrentPalette(uint8_t index = 0, uint8_t brightness = 255, TBlendType blendType = LINEARBLEND) {
  return ColorFromPalette(currentPalette, index, brightness, blendType);
}
/*
// Function to retrieve a color from the current palette
uint16_t getColorFromPalette(uint8_t index) {
  CRGB color = ColorFromPalette(currentPalette, index);
  return matrix.color565(color.r, color.g, color.b);
}
*/
// Spiral Colors
static unsigned long spiralLastUpdate = 0;
static float spiralCurrentAngle = 0;
static uint16_t spiralTime_counter = 0;
CRGBPalette16 spiralCurrentPalette = RainbowColors_p;


/* ----------------------------------------------------------------------
Matrix initialization is explained EXTENSIVELY in "simple" example sketch!
It's very similar here, but we're passing "true" for the last argument,
enabling double-buffering -- this permits smooth animation by having us
draw in a second "off screen" buffer while the other is being shown.
------------------------------------------------------------------------- */

Adafruit_Protomatter matrix(
  128,          // Matrix width in pixels
  6,           // Bit depth -- 6 here provides maximum color options
  1, rgbPins,  // # of matrix chains, array of 6 RGB pins for each
  4, addrPins, // # of address pins (height is inferred), array of pins
  clockPin, latchPin, oePin, // Other matrix control pins
  true);       // HERE IS THE MAGIC FOR DOUBLE-BUFFERING!

Adafruit_LIS3DH accel = Adafruit_LIS3DH();

  // Adjust this number for the sensitivity of the 'click' force
  // this strongly depend on the range! for 16G, try 5-10
  // for 8G, try 10-20. for 4G try 20-40. for 2G try 40-80
#define CLICKTHRESHHOLD 20

/////////////////////////// Button config
bool debounceButton(int pin) {
  static uint32_t lastPressTime = 0;
  uint32_t currentTime = millis();
  
  if (digitalRead(pin) == LOW && (currentTime - lastPressTime) > 200) {
    lastPressTime = currentTime;
    return true;
  }
  return false;
}

/////////////////////////// Bitmaps

// 'Apple_logo_black', 18x21px
const PROGMEM uint8_t appleLogoApple_logo_black[] = {
	0x00, 0x18, 0x00, 0x00, 0x30, 0x00, 0x00, 0x70, 0x00, 0x00, 0xe0, 0x00, 0x00, 0xc0, 0x00, 0x1e, 
	0x1e, 0x00, 0x7f, 0xff, 0x80, 0x7f, 0xff, 0x80, 0x7f, 0xfe, 0x00, 0xff, 0xfe, 0x00, 0xff, 0xfe, 
	0x00, 0xff, 0xfe, 0x00, 0xff, 0xfe, 0x00, 0xff, 0xfe, 0x00, 0xff, 0xff, 0x00, 0x7f, 0xff, 0x80, 
	0x7f, 0xff, 0x80, 0x3f, 0xff, 0x80, 0x3f, 0xff, 0x00, 0x1f, 0xfe, 0x00, 0x0f, 0x3c, 0x00
};

// Array of all bitmaps for convenience. (Total bytes used to store images in PROGMEM = 272)
const int appleLogoallArray_LEN = 1;
const unsigned char* appleLogoallArray[1] = {
	appleLogoApple_logo_black
};

/////////////////////////// Facial icons 

// Right side of the helmet
const PROGMEM uint8_t nose[] = {
    B00000000,
              B01111110,
              B00111111,
              B00000011,
              B00000011,
              B00000001,
              B00000000,
              B00000000
              };
const PROGMEM uint8_t maw[] = {
	0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x3f, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0xe3, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe3, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0xe0, 0x3f, 0x80, 0x00, 0x00, 0x00, 0xf0, 0x00, 0xe0, 0x3f, 0x80, 0x00, 0x00, 0x00, 0xf0, 0x00, 
	0xff, 0xff, 0xf0, 0x00, 0x00, 0x1f, 0xff, 0x00, 0xff, 0xff, 0xf0, 0x00, 0x00, 0x1f, 0xff, 0x00, 
	0x00, 0x00, 0x7f, 0x00, 0x00, 0xfc, 0x07, 0xe0, 0x00, 0x00, 0x7f, 0x00, 0x00, 0xfc, 0x07, 0xe0, 
	0x00, 0x00, 0x07, 0xf0, 0x0f, 0xe0, 0x00, 0xf8, 0x00, 0x00, 0x07, 0xf0, 0x0f, 0xe0, 0x00, 0xf8, 
	0x00, 0x00, 0x00, 0x3f, 0xfc, 0x00, 0x00, 0x1f, 0x00, 0x00, 0x00, 0x3f, 0xfc, 0x00, 0x00, 0x1f
             };
const PROGMEM uint8_t maw2[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x1f, 0x40, 0x00, 0x00, 0x00, 0x03, 0xc0, 0x00, 0x3f, 
	0xf0, 0x00, 0x00, 0x00, 0x07, 0xf8, 0x00, 0xff, 0xff, 0x00, 0x00, 0x00, 0x0f, 0xfc, 0x01, 0xff, 
	0xcf, 0xe0, 0x00, 0x00, 0x1f, 0xfe, 0x03, 0xff, 0xc7, 0xfe, 0x00, 0x00, 0x7f, 0xff, 0xc3, 0xff, 
	0xc3, 0xff, 0xe0, 0x00, 0xff, 0xff, 0xe7, 0xff, 0xc7, 0xff, 0xfe, 0x01, 0xff, 0xff, 0xff, 0xff, 
	0xcf, 0x3f, 0xff, 0xe7, 0xff, 0xff, 0xff, 0xff, 0x7f, 0x07, 0xff, 0xff, 0xff, 0x7f, 0xff, 0xff, 
	0x3c, 0x01, 0xff, 0xff, 0xfc, 0x3f, 0xff, 0xfe, 0x00, 0x00, 0x7f, 0xff, 0xf0, 0x3f, 0xff, 0xfc, 
	0x00, 0x00, 0x1f, 0xff, 0xe0, 0x0f, 0xff, 0xf0, 0x00, 0x00, 0x03, 0xff, 0x80, 0x07, 0xff, 0xe0, 
	0x00, 0x00, 0x00, 0xfe, 0x00, 0x03, 0xff, 0xc0, 0x00, 0x00, 0x00, 0x18, 0x00, 0x03, 0xff, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7c, 0x00
};
const PROGMEM uint8_t maw2Closed[] = {
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x38, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 
	0x7e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0xef, 0x80, 0x00, 0x00, 0x01, 0x80, 0x00, 0x07, 
	0xc1, 0xf0, 0x00, 0x00, 0x03, 0xc0, 0x00, 0x0f, 0xc3, 0xfc, 0x00, 0x00, 0x0f, 0xf0, 0x00, 0x1f, 
	0xef, 0x3f, 0x80, 0x00, 0x3f, 0xfc, 0x00, 0x3f, 0x7f, 0x0f, 0xe0, 0x00, 0xff, 0xfe, 0x00, 0xff, 
	0x3c, 0x03, 0xf8, 0x03, 0xf8, 0x7f, 0x87, 0xff, 0x00, 0x00, 0xfe, 0x0f, 0xe0, 0x3f, 0xff, 0xff, 
	0x00, 0x00, 0x3f, 0xff, 0x80, 0x1f, 0xff, 0xfc, 0x00, 0x00, 0x0f, 0xfe, 0x00, 0x07, 0xff, 0xe0, 
	0x00, 0x00, 0x03, 0xf0, 0x00, 0x01, 0xff, 0x80, 0x00, 0x00, 0x00, 0xc0, 0x00, 0x00, 0x7e, 0x00
};
const PROGMEM uint8_t Glitch1[] = {
    B00110000, B00010000, B00000100, B00000100,
                 B00000000, B00101000, B01100000, B00111010,
                 B00110001, B00101000, B00000001, B00010011,
                 B00000111, B00100010, B00100001, B10100001,
                 B01011101, B11110000, B00000011, B11111111,
                 B00111000, B01101001, B01001110, B00000100,
                 B10100000, B01001110, B01001100, B00000110,
                 B10000001, B00000111, B11100100, B00000000
                 };
const PROGMEM uint8_t Glitch2[] = {
    B00000000, B00000000, B00000000, B00000100,
                 B00000000, B00000000, B00000000, B00011110,
                 B00100000, B00010000, B00000000, B01001011,
                 B00000111, B00100000, B00000001, B11100111,
                 B00011111, B11110000, B00000110, B11111111,
                 B00001011, B11111101, B01111100, B00000000,
                 B11100110, B00010110, B01011000, B00000000,
                 B00000000, B00000111, B11100000, B00000000
                 };
const PROGMEM uint8_t Eye[] = {
	0x00, 0xff, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x0f, 0xff, 0xfc, 0x00, 0x0f, 0xff, 0xfc, 0x00, 
	0x3f, 0xff, 0xff, 0xc0, 0x3f, 0xff, 0xff, 0xc0, 0xff, 0xff, 0xff, 0xfc, 0xff, 0xff, 0xff, 0xfc, 
	0xff, 0x00, 0x00, 0x3f, 0xff, 0x00, 0x00, 0x3f, 0x3c, 0x00, 0x00, 0x03, 0x3c, 0x00, 0x00, 0x03, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
             };
const PROGMEM uint8_t Angry[] = {
        B00000000, B00000000,
               B00011111, B11111100,
               B00111111, B11111110,
               B00111111, B11111100,
               B00011111, B11111000,
               B00001111, B11100000,
               B00000011, B10000000,
               B00000000, B00000000
               };
const PROGMEM uint8_t Spooked[] = {
    B00000011, B11000000,
                 B00000111, B11100000,
                 B00001111, B11110000,
                 B00001111, B11110000,
                 B00001111, B11110000,
                 B00001111, B11110000,
                 B00000111, B11100000,
                 B00000011, B11000000
                 };
const PROGMEM uint8_t vwv[] = {
        B00001110, B00000000,
             B00000111, B10000000,
             B00000001, B11100000,
             B00000000, B01111000,
             B00000000, B01111000,
             B00000001, B11100000,
             B00000111, B10000000,
             B00001110, B00000000};
const PROGMEM uint8_t blush[] = {
	0x00, 0xc0, 0x00, 0xe0, 0x01, 0xc0, 0x03, 0x80, 0x07, 0x80, 0x07, 0x00, 0x0e, 0x00, 0x1c, 0x00, 
	0x3c, 0x00, 0x38, 0x00, 0x70, 0x00, 0xe0, 0x00, 0x60, 0x00
};
const PROGMEM uint8_t semicircleeyes[] = {
  	0xff, 0xff, 0xff, 0xfe, 0x7f, 0xff, 0xff, 0xfc, 0x7f, 0xff, 0xff, 0xfc, 0x7f, 0xff, 0xff, 0xfc, 
	0x3f, 0xff, 0xff, 0xf8, 0x3f, 0xff, 0xff, 0xf8, 0x1f, 0xff, 0xff, 0xf0, 0x0f, 0xff, 0xff, 0xe0, 
	0x07, 0xff, 0xff, 0xc0, 0x01, 0xff, 0xff, 0x00, 0x00, 0x7f, 0xfc, 0x00, 0x00, 0x07, 0xc0, 0x00
};
const PROGMEM uint8_t x_eyes[] = {
	  0xe0, 0x00, 0x00, 0x0e, 0xf8, 0x00, 0x00, 0x3e, 0x3e, 0x00, 0x00, 0xf8, 0x0f, 0xc0, 0x07, 0xe0, 
	0x03, 0xf0, 0x1f, 0x80, 0x00, 0x7c, 0x7c, 0x00, 0x00, 0x1f, 0xf0, 0x00, 0x00, 0x07, 0xc0, 0x00, 
	0x00, 0x1f, 0xf0, 0x00, 0x00, 0x7c, 0x7c, 0x00, 0x03, 0xf0, 0x1f, 0x80, 0x0f, 0xc0, 0x07, 0xe0, 
	0x3e, 0x00, 0x00, 0xf8, 0xf8, 0x00, 0x00, 0x3e, 0xe0, 0x00, 0x00, 0x0e
};
const PROGMEM uint8_t slanteyes[] = {
0x80, 0x00, 0x00, 0xf8, 0x00, 0x00, 0xff, 0x80, 0x00, 0xff, 0xf0, 0x00, 0xff, 0xff, 0x00, 0xff, 
	0xff, 0xf0, 0xff, 0xff, 0xff, 0x7f, 0xff, 0xff, 0x7f, 0xff, 0xff, 0x7f, 0xff, 0xff, 0x3f, 0xff, 
	0xfe, 0x1f, 0xff, 0xfc, 0x0f, 0xff, 0xf8, 0x07, 0xff, 0xf0, 0x03, 0xff, 0xe0, 0x00, 0x7f, 0x00
};
const PROGMEM uint8_t spiral[] = {
0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x0e, 0x00, 0x00, 0x00, 0xff, 0xc0, 0x00, 
	0x03, 0xff, 0xf0, 0x00, 0x07, 0xf1, 0xf8, 0x00, 0x0f, 0x80, 0x3c, 0x00, 0x1e, 0x00, 0x1e, 0x00, 
	0x3c, 0x00, 0x0f, 0x00, 0x38, 0x0e, 0x07, 0x00, 0x78, 0x3f, 0x83, 0x80, 0x70, 0x7f, 0xc3, 0x80, 
	0x70, 0xf1, 0xc3, 0x80, 0xe1, 0xe1, 0xc3, 0x80, 0xe1, 0xc7, 0xc3, 0x80, 0xe1, 0xc7, 0xc3, 0x80, 
	0xe1, 0xc7, 0xc3, 0x80, 0xe1, 0xc1, 0x07, 0x80, 0xe1, 0xe0, 0x07, 0x00, 0xf0, 0xe0, 0x0f, 0x00, 
	0x60, 0xf8, 0x3e, 0x00, 0x00, 0x7f, 0xfc, 0x00, 0x00, 0x3f, 0xf8, 0x00, 0x00, 0x0f, 0xe0, 0x00, 
	0x00, 0x00, 0x00, 0x00
  };
const PROGMEM uint8_t hearteyes[] = {
0x0f, 0x0f, 0x00, 0x3f, 0x9f, 0xc0, 0x7f, 0xff, 0xe0, 0x7f, 0xff, 0xe0, 0xff, 0xff, 0xf0, 0xff, 
	0xff, 0xf0, 0xff, 0xff, 0xf0, 0x7f, 0xff, 0xe0, 0x7f, 0xff, 0xe0, 0x3f, 0xff, 0xc0, 0x1f, 0xff, 
	0x80, 0x0f, 0xff, 0x00, 0x07, 0xfe, 0x00, 0x03, 0xfc, 0x00, 0x01, 0xf8, 0x00, 0x00, 0xf0, 0x00, 
	0x00, 0x60, 0x00
};

// Left side of the helmet
const PROGMEM uint8_t noseL[] = {
    B00000000,
               B01111110,
               B11111100,
               B11000000,
               B11000000,
               B10000000,
               B00000000,
               B00000000
               };
const PROGMEM uint8_t mawL[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0xfc, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 0xc7, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 0xc7, 
	0x00, 0x0f, 0x00, 0x00, 0x00, 0x01, 0xfc, 0x07, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x01, 0xfc, 0x07, 
	0x00, 0xff, 0xf8, 0x00, 0x00, 0x0f, 0xff, 0xff, 0x00, 0xff, 0xf8, 0x00, 0x00, 0x0f, 0xff, 0xff, 
	0x07, 0xe0, 0x3f, 0x00, 0x00, 0xfe, 0x00, 0x00, 0x07, 0xe0, 0x3f, 0x00, 0x00, 0xfe, 0x00, 0x00, 
	0x1f, 0x00, 0x07, 0xf0, 0x0f, 0xe0, 0x00, 0x00, 0x1f, 0x00, 0x07, 0xf0, 0x0f, 0xe0, 0x00, 0x00, 
	0xf8, 0x00, 0x00, 0x3f, 0xfc, 0x00, 0x00, 0x00, 0xf8, 0x00, 0x00, 0x3f, 0xfc, 0x00, 0x00, 0x00
              };
const PROGMEM uint8_t maw2L[] = {
  0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0xf8, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0x00, 0x03, 0xc0, 0x00, 0x00, 0x00, 0x02, 
	0xff, 0x00, 0x1f, 0xe0, 0x00, 0x00, 0x00, 0x0f, 0xff, 0x80, 0x3f, 0xf0, 0x00, 0x00, 0x00, 0xff, 
	0xff, 0xc0, 0x7f, 0xf8, 0x00, 0x00, 0x07, 0xf3, 0xff, 0xc3, 0xff, 0xfe, 0x00, 0x00, 0x7f, 0xe3, 
	0xff, 0xe7, 0xff, 0xff, 0x00, 0x07, 0xff, 0xc3, 0xff, 0xff, 0xff, 0xff, 0x80, 0x7f, 0xff, 0xe3, 
	0xff, 0xff, 0xff, 0xff, 0xe7, 0xff, 0xfc, 0xf3, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xff, 0xe0, 0xfe, 
	0x7f, 0xff, 0xfc, 0x3f, 0xff, 0xff, 0x80, 0x3c, 0x3f, 0xff, 0xfc, 0x0f, 0xff, 0xfe, 0x00, 0x00, 
	0x0f, 0xff, 0xf0, 0x07, 0xff, 0xf8, 0x00, 0x00, 0x07, 0xff, 0xe0, 0x01, 0xff, 0xc0, 0x00, 0x00, 
	0x03, 0xff, 0xc0, 0x00, 0x7f, 0x00, 0x00, 0x00, 0x00, 0xff, 0xc0, 0x00, 0x18, 0x00, 0x00, 0x00, 
	0x00, 0x7f, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
const PROGMEM uint8_t maw2ClosedL[] = {
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1c, 
	0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7e, 0xe0, 0x00, 0x01, 0x80, 0x00, 0x00, 0x01, 0xf7, 
	0xf0, 0x00, 0x03, 0xc0, 0x00, 0x00, 0x0f, 0x83, 0xf8, 0x00, 0x0f, 0xf0, 0x00, 0x00, 0x3f, 0xc3, 
	0xfc, 0x00, 0x3f, 0xfc, 0x00, 0x01, 0xfc, 0xf7, 0xff, 0x00, 0x7f, 0xff, 0x00, 0x07, 0xf0, 0xfe, 
	0xff, 0xe1, 0xfe, 0x1f, 0xc0, 0x1f, 0xc0, 0x3c, 0xff, 0xff, 0xfc, 0x07, 0xf0, 0x7f, 0x00, 0x00, 
	0x3f, 0xff, 0xf8, 0x01, 0xff, 0xfc, 0x00, 0x00, 0x07, 0xff, 0xe0, 0x00, 0x7f, 0xf0, 0x00, 0x00, 
	0x01, 0xff, 0x80, 0x00, 0x0f, 0xc0, 0x00, 0x00, 0x00, 0x7e, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00
};
const PROGMEM uint8_t Glitch1L[] = {
        B00000000, B00000000, B00000000, B00000100,
                  B00000000, B00000000, B00000000, B00011110,
                  B00100000, B00010000, B00000000, B01001011,
                  B00000111, B00100000, B00000001, B11100111,
                  B00011111, B11110000, B00000110, B11111111,
                  B00001011, B11111101, B01111100, B00000000,
                  B11100110, B00010110, B01011000, B00000000,
                  B00000000, B00000111, B11100000, B00000000
                  };
const PROGMEM uint8_t Glitch2L[] = {
    0x0c, 0x08, 0x20, 0x20, 0x00, 0x14, 0x06, 0x5c, 0x0c, 0x14, 0x80, 0xc8, 0xe0,
                  0x22, 0xc2, 0xc2, 0xba, 0x0f, 0xc0, 0xff, 0x1c, 0x96, 0x72, 0x20, 0x05, 0x72,
                  0x32, 0x60, 0x81, 0xe0, 0x27, 0x00
                  };
const PROGMEM uint8_t EyeL[] = {
	0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x3f, 0xff, 0xf0, 0x00, 0x3f, 0xff, 0xf0, 
	0x03, 0xff, 0xff, 0xfc, 0x03, 0xff, 0xff, 0xfc, 0x3f, 0xff, 0xff, 0xff, 0x3f, 0xff, 0xff, 0xff, 
	0xfc, 0x00, 0x00, 0xff, 0xfc, 0x00, 0x00, 0xff, 0xc0, 0x00, 0x00, 0x3c, 0xc0, 0x00, 0x00, 0x3c, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
              };
const PROGMEM uint8_t AngryL[] = {
    B00000000, B00000000,
                B00111111, B11111000,
                B01111111, B11111100,
                B00111111, B11111100,
                B00011111, B11111000,
                B00000111, B11110000,
                B00000001, B11000000,
                B00000000, B00000000
                };
const PROGMEM uint8_t SpookedL[] = {
    B00000011, B11000000,
                  B00000111, B11100000,
                  B00001111, B11110000,
                  B00001111, B11110000,
                  B00001111, B11110000,
                  B00001111, B11110000,
                  B00000111, B11100000,
                  B00000011, B11000000
                  };
const PROGMEM uint8_t vwvL[] = {
    B00000000, B01110000,
              B00000001, B11100000,
              B00000111, B10000000,
              B00011110, B00000000,
              B00011110, B00000000,
              B00000111, B10000000,
              B00000001, B11100000,
              B00000000, B01110000
              };
const PROGMEM uint8_t blushL[] = {
	0x60, 0x00, 0xe0, 0x00, 0x70, 0x00, 0x38, 0x00, 0x3c, 0x00, 0x1c, 0x00, 0x0e, 0x00, 0x07, 0x00, 
	0x07, 0x80, 0x03, 0x80, 0x01, 0xc0, 0x00, 0xe0, 0x00, 0xc0
  };
const PROGMEM uint8_t slanteyesL[] = {
0x00, 0x00, 0x01, 0x00, 0x00, 0x1f, 0x00, 0x01, 0xff, 0x00, 0x0f, 0xff, 0x00, 0xff, 0xff, 0x0f, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xfe, 0x7f, 0xff, 
	0xfc, 0x3f, 0xff, 0xf8, 0x1f, 0xff, 0xf0, 0x0f, 0xff, 0xe0, 0x07, 0xff, 0xc0, 0x00, 0xfe, 0x00
  };
const PROGMEM uint8_t spiralL[] = {
0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x38, 0x00, 0x00, 0x01, 0xff, 0x80, 0x00, 
	0x07, 0xff, 0xe0, 0x00, 0x0f, 0xc7, 0xf0, 0x00, 0x1e, 0x00, 0xf8, 0x00, 0x3c, 0x00, 0x3c, 0x00, 
	0x78, 0x00, 0x1e, 0x00, 0x70, 0x38, 0x0e, 0x00, 0xe0, 0xfe, 0x0f, 0x00, 0xe1, 0xff, 0x07, 0x00, 
	0xe1, 0xc7, 0x87, 0x00, 0xe1, 0xc3, 0xc3, 0x80, 0xe1, 0xf1, 0xc3, 0x80, 0xe1, 0xf1, 0xc3, 0x80, 
	0xe1, 0xf1, 0xc3, 0x80, 0xf0, 0x41, 0xc3, 0x80, 0x70, 0x03, 0xc3, 0x80, 0x78, 0x03, 0x87, 0x80, 
	0x3e, 0x0f, 0x83, 0x00, 0x1f, 0xff, 0x00, 0x00, 0x0f, 0xfe, 0x00, 0x00, 0x03, 0xf8, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00
};
// Buffer icons for transitions
uint8_t bufferL[] = {
    B00000000, B00000000,
                     B00000000, B00000000,
                     B00000000, B00000000,
                     B00000000, B00000000,
                     B00000000, B00000000,
                     B00000000, B00000000,
                     B00000000, B00000000,
                     B00000000, B00000000
                     };
uint8_t bufferR[] = {
    B00000000, B00000000,
                     B00000000, B00000000,
                     B00000000, B00000000,
                     B00000000, B00000000,
                     B00000000, B00000000,
                     B00000000, B00000000,
                     B00000000, B00000000,
                     B00000000, B00000000
                     };
uint8_t fftIcon1[] = {
    B00000000, B00000000,
                      B00000000, B00000000,
                      B00000000, B00000000,
                      B00000000, B00000000,
                      B00000000, B00000000,
                      B00000000, B00000000,
                      B00000000, B00000000,
                      B00000000, B00000000
                      };
uint8_t fftIcon2[] = {
    B00000000, B00000000,
                      B00000000, B00000000,
                      B00000000, B00000000,
                      B00000000, B00000000,
                      B00000000, B00000000,
                      B00000000, B00000000,
                      B00000000, B00000000,
                      B00000000, B00000000
                      };
uint8_t fftIcon1L[] = {
    B00000000, B00000000,
                       B00000000, B00000000,
                       B00000000, B00000000,
                       B00000000, B00000000,
                       B00000000, B00000000,
                       B00000000, B00000000,
                       B00000000, B00000000,
                       B00000000, B00000000
                       };
uint8_t fftIcon2L[] = {
    B00000000, B00000000,
                       B00000000, B00000000,
                       B00000000, B00000000,
                       B00000000, B00000000,
                       B00000000, B00000000,
                       B00000000, B00000000,
                       B00000000, B00000000,
                       B00000000, B00000000
                       };


// Sundry globals used for animation ---------------------------------------

int16_t  textX;        // Current text position (X)
int16_t  textY;        // Current text position (Y)
int16_t  textMin;      // Text pos. (X) when scrolled off left edge
char     str[64];      // Buffer to hold scrolling message text
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
  for(i=1;;i++) {                     // Loop forever...
    digitalWrite(LED_BUILTIN, i & 1); // LED on/off blink to alert user
    delay(x);
  }
}

void displayLoadingBar() {
  // Draw Apple Logos
  matrix.drawBitmap(23, 2, appleLogoApple_logo_black, 18, 21, matrix.color565(255, 255, 255));
  matrix.drawBitmap(88, 2, appleLogoApple_logo_black, 18, 21, matrix.color565(255, 255, 255));

  // Draw the loading bar background
  int barWidth = matrix.width() - 80; // Width of the loading bar
  int barHeight = 5;                // Height of the loading bar
  int barX = (matrix.width() / 4) - (barWidth / 2);        // Center X position
  int barY = (matrix.height() - barHeight) / 2; // Center vertically
  int barRadius = 4;                // Rounded corners
  matrix.fillRoundRect(barX, (barY * 2), barWidth, barHeight, barRadius, matrix.color565(9, 9, 9));
  matrix.fillRoundRect(barX + 64, (barY * 2), barWidth, barHeight, barRadius, matrix.color565(9, 9, 9));

  // Draw the loading progress
  int progressWidth = (barWidth - 2) * loadingProgress / loadingMax;
  matrix.fillRoundRect(barX + 1, (barY * 2) + 1, progressWidth, barHeight - 2, barRadius - 2, matrix.color565(255, 255, 255));
  matrix.fillRoundRect((barX + 1) + 64, (barY * 2) + 1, progressWidth, barHeight - 2, barRadius - 2, matrix.color565(255, 255, 255));

/*
  // Display percentage text
  char progressText[16];
  sprintf(progressText, "%d%%", loadingProgress);
  matrix.setTextColor(matrix.color565(255, 255, 255));
  matrix.setCursor(barX - 8, barY + 14); // Position above the bar
  matrix.print(progressText);
*/
  matrix.show(); // Update the matrix display
  delay(10);     // Short delay for smoother animation
}

void updateBlinkAnimation() {
  unsigned long currentTime = millis();

  if (isBlinking) {
    unsigned long elapsed = currentTime - lastEyeBlinkTime;

    if (elapsed >= blinkDuration) {
      // End the blink animation
      isBlinking = false;
      lastEyeBlinkTime = currentTime;
      nextBlinkDelay = random(minBlinkDelay, maxBlinkDelay); // Set random delay for the next blink
    } else {
      // Update non-linear blink progress (0 to 100%)
      float normalizedProgress = float(elapsed) / blinkDuration;
      
      // Smooth easing for closing and opening
      if (normalizedProgress < 0.5) {
        // Closing phase
        blinkProgress = 100 * easeInOutQuad(normalizedProgress * 2);
      } else {
        // Opening phase
        blinkProgress = 100 * (1 - easeInOutQuad((normalizedProgress - 0.5) * 2));
      }
    }
  } else {
    // Start a new blink after the random delay
    if (currentTime - lastEyeBlinkTime >= nextBlinkDelay) {
      isBlinking = true;
      blinkProgress = 0;
      lastEyeBlinkTime = currentTime;
      blinkDuration = random(minBlinkDuration, maxBlinkDuration); // Randomize blink duration for variety
    }
  }
}

// Helper function for ease-in-out quadratic easing
float easeInOutQuad(float t) {
  if (t < 0.5) {
    return 2 * t * t;
  }
  return -1 + (4 - 2 * t) * t;
}

// Function for rotating bitmap
void drawRotatedBitmap(int16_t x, int16_t y, const uint8_t *bitmap, uint16_t width, uint16_t height, float angle, uint16_t color) {
  // Convert angle from degrees to radians
  float radians = angle * PI / 180.0;
  float cosA = cos(radians);
  float sinA = sin(radians);

  // Calculate the center of the bitmap
  int16_t centerX = width / 2;
  int16_t centerY = height / 2;

  // Iterate through the bitmap's rows and columns
  for (int16_t j = 0; j < height; j++) {
    for (int16_t i = 0; i < width; i++) {
      // Check if the pixel is set in the bitmap
      if (bitmap[j * ((width + 7) / 8) + (i / 8)] & (1 << (7 - (i % 8)))) {
        // Calculate the offset from the center
        int16_t offsetX = i - centerX;
        int16_t offsetY = j - centerY;
        // Apply rotation
        int16_t newX = cosA * offsetX - sinA * offsetY + x;
        int16_t newY = sinA * offsetX + cosA * offsetY + y;

        // Check bounds and draw the pixel
        if (newX >= 0 && newX < matrix.width() && newY >= 0 && newY < matrix.height()) {
          matrix.drawPixel(newX, newY, color);
        }
      }
    }
  }
}

void drawScaledBitmap(int16_t x, int16_t y, const uint8_t* bitmap, int16_t width, int16_t height, int16_t scale, uint16_t color) {
    for (int16_t j = 0; j < height; j++) {
        for (int16_t i = 0; i < width; i++) {
            if (bitmap[j * ((width + 7) / 8) + (i / 8)] & (1 << (7 - (i % 8)))) {
                for (int16_t dy = 0; dy < scale; dy++) {
                    for (int16_t dx = 0; dx < scale; dx++) {
                        matrix.drawPixel(x + i * scale + dx, y + j * scale + dy, color);
                    }
                }
            }
        }
    }
}

// Spiral update function
void updateRotatingSpiral() {
  unsigned long currentTime = millis();
  static unsigned long lastUpdate = 0;
  static float currentAngle = 0;
  static uint16_t time_counter = 0;
  if (currentTime - lastUpdate > 0) { // Update every 30 ms
    lastUpdate = currentTime;
    currentAngle += 10.0; // Increment the angle for rotation
    if (currentAngle >= 360.0) {
      currentAngle -= 360.0; // Wrap angle to 0-359°
    }
    time_counter++; // Increment time_counter to change colors over time

    uint8_t paletteIndex = (time_counter / 4) % 255;
    CRGB currentColor = ColorFromPalette(currentPalette, paletteIndex);
    uint16_t color = matrix.color565(currentColor.r, currentColor.g, currentColor.b);

    // Clear the area where the spirals are drawn
    matrix.fillRect(11, 1, 27, 27, 0);
    matrix.fillRect(91, 1, 27, 27, 0);
    // Draw the rotating spirals
    drawRotatedBitmap(24, 15, spiral, 25, 25, currentAngle, color);
    drawRotatedBitmap(105, 15, spiralL, 25, 25, -currentAngle, color);

    //matrix.show();
    Serial.println(millis() - lastUpdate);
  }
}
/*
#define PALETTE_SIZE 256
uint16_t paletteCache[PALETTE_SIZE];

void cachePaletteColors() {
  for (int i = 0; i < PALETTE_SIZE; i++) {
    CRGB currentColor = ColorFromPalette(currentPalette, i);
    paletteCache[i] = matrix.color565(currentColor.r, currentColor.g, currentColor.b);
  }
  Serial.println("Palette cached.");
}

uint16_t getCachedColor(uint8_t index) {
  return paletteCache[index];
}
*/

// Draw the blinking eyes
void blinkingEyes() {
    
  // Draw  eyes
  if (currentView == 4 || currentView == 5) {
  matrix.drawBitmap(0, 0, Eye, 32, 16, matrix.color565(255, 255, 255));
  matrix.drawBitmap(96, 0, EyeL, 32, 16, matrix.color565(255, 255, 255));
  }
  if (currentView == 6) {
    matrix.drawBitmap(0, 0, semicircleeyes, 32, 12, matrix.color565(255, 255, 255));
    matrix.drawBitmap(96, 0, semicircleeyes, 32, 12, matrix.color565(255, 255, 255));
  }
  if (currentView == 7) {
    matrix.drawBitmap(0, 0, x_eyes, 31, 15, matrix.color565(255, 255, 255));
    matrix.drawBitmap(96, 0, x_eyes, 31, 15, matrix.color565(255, 255, 255));
  }
  /*
  if (currentView == 8) {
    matrix.drawBitmap(0, 0, Angry, 16, 8, matrix.color565(255, 255, 255));
    matrix.drawBitmap(96, 0, AngryL, 16, 8, matrix.color565(255, 255, 255));
  }
  if (currentView == 9) {
    matrix.drawBitmap(0, 0, Spooked, 16, 8, matrix.color565(255, 255, 255));
    matrix.drawBitmap(96, 0, SpookedL, 16, 8, matrix.color565(255, 255, 255));
  }
  */
  if (currentView == 8) {
    matrix.drawBitmap(0, 0, slanteyes, 24, 16, matrix.color565(255, 255, 255));
    matrix.drawBitmap(104, 0, slanteyesL, 24, 16, matrix.color565(255, 255, 255));
  }

  if (isBlinking) {
    // Calculate the height of the black box
    int boxHeight = map(blinkProgress, 0, 100, 0, 16); // From 0 to 16 pixels

    // Draw black boxes over the eyes
    matrix.fillRect(0, 0, 32, boxHeight, 0);     // Cover the right eye
    matrix.fillRect(96, 0, 32, boxHeight, 0);    // Cover the left eye
  }

  //matrix.show(); // Update the display
  }
  
void drawBlush() {
  // Calculate blush brightness
  if (isBlushFadingIn) {
    unsigned long currentTime = millis();
    unsigned long elapsedTime = currentTime - blushFadeStartTime;
  // Calculate blush brightness
  if (elapsedTime <= blushFadeDuration) {
    blushBrightness = map(elapsedTime, 0, blushFadeDuration, 0, 255); // Gradual fade-in
  } else {
      blushBrightness = 255; // Max brightness reached
      isBlushFadingIn = false; // Stop fading
    }
  }

  // Set blush color based on brightness
  uint16_t blushColor = matrix.color565(blushBrightness, 0, blushBrightness);

     matrix.drawBitmap(45, 1, blush, 11, 13, blushColor);
     matrix.drawBitmap(40, 1, blush, 11, 13, blushColor);
     matrix.drawBitmap(35, 1, blush, 11, 13, blushColor);
     matrix.drawBitmap(72, 1, blushL, 11, 13, blushColor);
     matrix.drawBitmap(77, 1, blushL, 11, 13, blushColor);
     matrix.drawBitmap(82, 1, blushL, 11, 13, blushColor);
}

void heartbeat() {
    unsigned long currentTime = millis();
    static float heartScale = 1.0; // Use float for smoother scaling
    static bool isScalingUp = true; // Track scaling direction

    if (currentTime - lastHeartUpdate > 30) { // Faster updates for smooth animation
        lastHeartUpdate = currentTime;

        // Clear previous hearts
        int heartWidth = 20 * heartScale;
        int heartHeight = 17 * heartScale;

         // Clear previous hearts
        matrix.fillRect(
            (matrix.width() / 2 - 20 * heartScale) / 2, 
            (matrix.height() - 17 * heartScale) / 2, 
            20 * heartScale, 
            17 * heartScale, 
            0
        );
        matrix.fillRect(
            (matrix.width() / 2 + (matrix.width() / 2 - 20 * heartScale) / 2), 
            (matrix.height() - 17 * heartScale) / 2, 
            20 * heartScale, 
            17 * heartScale, 
            0
        );

        // Update scaling with smooth transition
        if (isScalingUp) {
            heartScale += 0.1; // Increment by a small value
            if (heartScale >= 2.0) { // Max scale
                isScalingUp = false;
            }
        } else {
            heartScale -= 0.1; // Decrement by a small value
            if (heartScale <= 1.0) { // Min scale
                isScalingUp = true;
            }
        }

        // Draw scaled hearts on both screens
        // Draw scaled hearts on both screens
        drawScaledBitmap(
            (matrix.width() / 2 - 20 * heartScale) / 2, 
            (matrix.height() - 17 * heartScale) / 2, 
            hearteyes, 
            20, 
            17, 
            heartScale, 
            matrix.color565(255, 0, 0)
        );
        drawScaledBitmap(
            (matrix.width() / 2 + (matrix.width() / 2 - 20 * heartScale) / 2), 
            (matrix.height() - 17 * heartScale) / 2, 
            hearteyes, 
            20, 
            17, 
            heartScale, 
            matrix.color565(255, 0, 0)
        );

        matrix.show();
    }
}

void drawTransFlag() {
  int stripeHeight = matrixHeight / 5; // Height of each stripe

  // Define colors in RGB565 format
  uint16_t lightBlue = matrix.color565(0, 102/2, 204/2); // Deeper light blue
  uint16_t pink = matrix.color565(255, 20/2, 147/2);    // Deeper pink
  uint16_t white = matrix.color565(255/2, 255/2, 255/2);   // White (unchanged)

  // Draw stripes
  matrix.fillRect(0, 0, matrixWidth, stripeHeight, lightBlue);       // Top light blue stripe
  matrix.fillRect(0, stripeHeight, matrixWidth, stripeHeight, pink); // Pink stripe
  matrix.fillRect(0, stripeHeight * 2, matrixWidth, stripeHeight, white); // Middle white stripe
  matrix.fillRect(0, stripeHeight * 3, matrixWidth, stripeHeight, pink);  // Pink stripe
  matrix.fillRect(0, stripeHeight * 4, matrixWidth, stripeHeight, lightBlue); // Bottom light blue stripe
}

void protoFace() {
  uint8_t click = accel.getClick();

   matrix.drawBitmap(56, 2, nose, 8, 8, matrix.color565(255, 255, 255));
   matrix.drawBitmap(64, 2, noseL, 8, 8, matrix.color565(255, 255, 255));
   //Testing new maw shape
   //matrix.drawBitmap(0, 16, maw, 64, 16, matrix.color565(255, 255, 255));
   //matrix.drawBitmap(64, 16, mawL, 64, 16, matrix.color565(255, 255, 255));

   //matrix.drawBitmap(0, 0, Eye, 32, 16, matrix.color565(255, 255, 255));
   //matrix.drawBitmap(96, 0, EyeL, 32, 16, matrix.color565(255, 255, 255));

    /*
     matrix.drawBitmap(35, 12, blush, 11, 13, matrix.color565(255, 0, 255));
     matrix.drawBitmap(30, 12, blush, 11, 13, matrix.color565(255, 0, 255));
     matrix.drawBitmap(25, 12, blush, 11, 13, matrix.color565(255, 0, 255));
     matrix.drawBitmap(82, 12, blushL, 11, 13, matrix.color565(255, 0, 255));
     matrix.drawBitmap(87, 12, blushL, 11, 13, matrix.color565(255, 0, 255));
     matrix.drawBitmap(92, 12, blushL, 11, 13, matrix.color565(255, 0, 255));
     */

   // Draw the blush (with fading effect in view 5)
   if (currentView == 5) {
   drawBlush();
}
   
   // Draw blinking eyes
   blinkingEyes();

   //matrix.show(); // Update the matrix display
}

void patternPlasma() {
  for (int x = 0; x < matrix.width() * 1; x++) {
    for (int y = 0; y < matrix.height(); y++) {
      int16_t v = 128;
      uint8_t wibble = sin8(time_counter);
      v += sin16(x * wibble * 3 + time_counter);
      v += cos16(y * (128 - wibble) + time_counter);
      v += sin16(y * x * cos8(-time_counter) / 8);

      currentColor = ColorFromPalette(currentPalette, (v >> 8));
      uint16_t color = matrix.color565(currentColor.r, currentColor.g, currentColor.b);
      matrix.drawPixel(x, y, color);
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

  // Refresh display
  matrix.show();
}

void setup(void) {
  Serial.begin(115200);

  // Initialize matrix...
  ProtomatterStatus status = matrix.begin();
  Serial.print("Protomatter begin() status: ");
  Serial.println((int)status);
  if(status != PROTOMATTER_OK) {
    // DO NOT CONTINUE if matrix setup encountered an error.
    for(;;);
  }
randomSeed(analogRead(0)); // Seed the random number generator for randomized eye blinking

  // Unlike the "simple" example, we don't do any drawing in setup().
  // But we DO initialize some things we plan to animate...

  // Set up the scrolling message...
  sprintf(str, "LumiFur_02 %dx%d RGB LED Matrix",
    matrix.width(), matrix.height());
  matrix.setFont(&FreeSansBold18pt7b); // Use nice bitmap font
  matrix.setTextWrap(false);           // Allow text off edge
  matrix.setTextColor(0xFFFF);         // White
  int16_t  x1, y1;
  uint16_t w, h;
  matrix.getTextBounds(str, 0, 0, &x1, &y1, &w, &h); // How big is it?
  textMin = -w; // All text is off left edge when it reaches this point
  textX = matrix.width(); // Start off right edge
  textY = matrix.height() / 2 - (y1 + h / 2); // Center text vertically
  // Note: when making scrolling text like this, the setTextWrap(false)
  // call is REQUIRED (to allow text to go off the edge of the matrix),
  // AND it must be BEFORE the getTextBounds() call (or else that will
  // return the bounds of "wrapped" text).

  // Set up the colors of the bouncy balls.
  ballcolor[0] = matrix.color565(0, 20, 0); // Dark green
  ballcolor[1] = matrix.color565(0, 0, 20); // Dark blue
  ballcolor[2] = matrix.color565(20, 0, 0); // Dark red

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

// Confugure LIS3DH accelerometer if present (Built-in on MatrixPortal)
#if defined(ARDUINO_ADAFRUIT_MATRIXPORTAL_ESP32S3)
// Initialize LIS3DH
  if (! accel.begin(0x19)) { // 0x18 is the default I2C address for LIS3DH
    Serial.println("Could not find LIS3DH!");
    while (1) yield(); // Loop forever
  }
  Serial.println("LIS3DH found!");

  // Set data rate to 400 Hz
  //accel.setDataRate(LIS3DH_DATARATE_400_HZ);

  // Set range to ±2G
  accel.setRange(LIS3DH_RANGE_2_G); // 2, 4, 8 or 16 G

  // Configure tap detection
  // Single-tap: tap detection on all axes
  accel.setClick(1, CLICKTHRESHHOLD); // 1 = single-tap, threshold = 20
  delay(100); // Give time for accelerometer to settle

  // Uncomment below for double-tap detection
  // accel.setClick(2, 20, 80, 200); // taps = 2, threshold = 20, latency = 80 ms, window = 200 ms
#endif
}

  void displayCurrentMaw() {
  switch (currentMaw) {
    case 0:
    // Closed maw
    Serial.println("Displaying closed maw");
    matrix.fillRect(0, 10, 128, 22, matrix.color565(0, 0, 0)); // Clear the maw area
    matrix.drawBitmap(0, 10, maw2Closed, 64, 22, matrix.color565(255, 255, 255));
    matrix.drawBitmap(64, 10, maw2ClosedL, 64, 22, matrix.color565(255, 255, 255));
    break;
    
    case 1:
    //Opened maw
    Serial.println("Displaying open maw");
    matrix.fillRect(0, 10, 128, 22, matrix.color565(0, 0, 0)); // Clear the maw area
    matrix.drawBitmap(0, 10, maw2, 64, 22, matrix.color565(255, 255, 255));
    matrix.drawBitmap(64, 10, maw2L, 64, 22, matrix.color565(255, 255, 255));
    break;

    default:
      Serial.println("Unknown Maw State");
      break;
  }
  matrix.show();
  }

void displayCurrentView(int view) {
  static int previousView = -1; // Track the last active view
  //matrix.fillScreen(0); // Clear display buffer at start of each frame

if (view != previousView) {
      if (view == 5) {
      // Reset fade logic when entering the blush view
      blushFadeStartTime = millis();
      isBlushFadingIn = true;
    }
    previousView = view; // Update the last active view
    matrix.fillScreen(0); // Clear display buffer at start of each frame
  }

  switch (view) {

  case 0: // Scrolling text Debug View
    // Every frame, we clear the background and draw everything anew.
    // This happens "in the background" with double buffering, that's
    // why you don't see everything flicker. It requires double the RAM,
    // so it's not practical for every situation.
    matrix.fillScreen(0); // Clear display buffer at start of each frame
    // Draw big scrolling text
    matrix.setCursor(textX, textY);
    matrix.print(str);

    // Update text position for next frame. If text goes off the
    // left edge, reset its position to be off the right edge.
    if ((--textX) < textMin)
      textX = matrix.width();

    // Draw the three bouncy balls on top of the text...
    for (byte i = 0; i < 3; i++)
    {
      // Draw 'ball'
      matrix.fillCircle(ball[i][0], ball[i][1], 5, ballcolor[i]);
      // Update ball's X,Y position for next frame
      ball[i][0] += ball[i][2];
      ball[i][1] += ball[i][3];
      // Bounce off edges
      if ((ball[i][0] == 0) || (ball[i][0] == (matrix.width() - 1)))
        ball[i][2] *= -1;
      if ((ball[i][1] == 0) || (ball[i][1] == (matrix.height() - 1)))
        ball[i][3] *= -1;
    }
    break;

  case 1: // Loading bar effect
    displayLoadingBar();
    if (loadingProgress <= loadingMax)
    {
      displayLoadingBar(); // Update the loading bar
      loadingProgress++;   // Increment progress
      delay(50);           // Adjust speed of loading animation
    }
    else
    {
      // Reset or transition to another view
      loadingProgress = 0;
    }
    break;

  case 2: // Pattern plasma
    patternPlasma();
    // delay(10);
    break;

  case 3:
    drawTransFlag();
    delay(20);
    break;

  case 4: // Normal
    protoFace();
    updateBlinkAnimation(); // Update blink animation progress
    //delay(20);              // Short delay for smoother animation
    break;

  case 5: // Blush with fade in effect
    protoFace();
    updateBlinkAnimation(); // Update blink animation progress
    break;

  case 6: // Dialated pupils
    protoFace();
    updateBlinkAnimation(); // Update blink animation progress
    break;

  case 7: // X eyes
    protoFace();
    updateBlinkAnimation(); // Update blink animation progress
    break;

  case 8: // Slant eyes
    protoFace();
    updateBlinkAnimation(); // Update blink animation progress
    break;

  case 9: // Spiral eyes
    //matrix.fillScreen(0); // Clear display buffer at start of each frame
    protoFace();
    //displayCurrentMaw();  // Ensure the maw is displayed
    matrix.fillRect(0, 10, 128, 22, matrix.color565(0, 0, 0)); // Clear the maw area
    updateRotatingSpiral();
    matrix.drawBitmap(0, 10, maw2Closed, 64, 22, matrix.color565(255, 255, 255));
    matrix.drawBitmap(64, 10, maw2ClosedL, 64, 22, matrix.color565(255, 255, 255));
    //updateRotatingSpiral();
    break;
  case 10: //Heart eyes
    protoFace();
    heartbeat(); //Beating heart eyes effect
    break;
/*
  case 10: // Slant eyes
    protoFace();
    updateBlinkAnimation(); // Update blink animation progress
    break;
  
  case 11: // Spiral eyes
    protoFace();
    updateBlinkAnimation(); // Update blink animation progress
    break;
*/
  }
  matrix.show(); // Update the display with the current buffer
}

// LOOP - RUNS REPEATEDLY AFTER SETUP --------------------------------------

void loop(void) {
  // Non-blocking display update for the current view
  static unsigned long lastFrameTime = 0;
  unsigned long currentTime = millis();

  // Target frame duration (e.g., 30 FPS = ~33ms per frame)
  const unsigned long frameDuration = 11;
  
  // Shake detection variables
  static unsigned long lastShakeTime = 0;
  const unsigned long shakeCooldown = 2000; // 2 seconds cooldown
  const float shakeThreshold = 60.0; // Acceleration threshold for shake detection
  static int previousView = -1; // To store the view before the spiral effect

// Check if it's time for the next frame
  if (currentTime - lastFrameTime >= frameDuration) {
    lastFrameTime = currentTime;

    if (debounceButton(BUTTON_UP)) {
      currentView = (currentView + 1) % totalViews;
      Serial.print("Button UP pressed. Current view: ");
      Serial.println(currentView);
    }
    if (debounceButton(BUTTON_DOWN)) {
      currentView = (currentView - 1 + totalViews) % totalViews;
      Serial.print("Button DOWN pressed. Current view: ");
      Serial.println(currentView);
    }

    displayCurrentView(currentView);
  }

// Check for tap for temporary maw change
  uint8_t clickType = accel.getClick();
  if (currentView != 0 && currentView != 1 && currentView != 2 && 
    currentView != 3 && currentView != 9 && currentView != 10) {
  if (clickType == 1) { // Single tap detected
    Serial.println("Single Tap Detected!");
    currentMaw = 1;                 // Switch to open maw
    mawTemporaryChange = true;      // Mark the change as temporary
    mawChangeTime = currentTime;    // Record the current time
    displayCurrentMaw();            // Update display
  }

  // Check if 1 second has passed since the temporary change
  if (mawTemporaryChange && (currentTime - mawChangeTime >= 1000)) {
    currentMaw = 0;                 // Revert to closed maw
    mawTemporaryChange = false;     // Reset temporary change flag
    displayCurrentMaw();            // Update display
  }
  } else {
    // Disable tap detection and show closed maw in the specified views
  if (currentMaw != 0) { // Ensure maw is closed
    currentMaw = 0;                 // Set maw to closed
    mawTemporaryChange = false;     // Reset any temporary change
    displayCurrentMaw();            // Update display
    Serial.println("Tap detection disabled. Maw set to closed.");
  }
  }
  // Add a small delay for stability
  //delay(50);

  // Detect shake for triggering spiral
  sensors_event_t event;
  accel.getEvent(&event); // Get current acceleration data
  if ((currentTime - lastShakeTime >= shakeCooldown) &&
        (abs(event.acceleration.x) > shakeThreshold || 
        abs(event.acceleration.y) > shakeThreshold || 
        abs(event.acceleration.z) > shakeThreshold)) {
    Serial.println("Shake Detected! Triggering Spiral...");
  // Store the current view and switch to the spiral effect
    if (currentView != 9) { // Avoid overwriting if already in spiral view
      previousView = currentView;
    }
    currentView = 9; // Set to spiral view
    displayCurrentView(currentView); // Immediately display spiral
    lastShakeTime = currentTime; // Record the time of this shake
  }

  // Return to the previous view after the cooldown
  if (currentView == 9 && (currentTime - lastShakeTime >= shakeCooldown)) {
    if (previousView != -1) { // Check if a previous view is stored
      currentView = previousView; // Restore the previous view
      previousView = -1; // Reset the stored view
      displayCurrentView(currentView); // Update display
      Serial.print("Returning to previous view: ");
      Serial.println(currentView);
    }
  }
}
