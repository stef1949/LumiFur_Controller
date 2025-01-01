// How to use this library with a FM6126 panel, thanks goes to:
// https://github.com/hzeller/rpi-rgb-led-matrix/issues/746

#ifdef IDF_BUILD
#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/gpio.h>
#include "sdkconfig.h"
#endif

#include <Arduino.h>
#include "xtensa/core-macros.h"
#ifdef VIRTUAL_PANE
#include <ESP32-VirtualMatrixPanel-I2S-DMA.h>
#else
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#endif
#include "main.h"

// HUB75E pinout
// R1 | G1
// B1 | GND
// R2 | G2
// B2 | E
//  A | B
//  C | D
// CLK| LAT
// OE | GND

/*  Default library pin configuration for the reference
  you can redefine only ones you need later on object creation
#define R1 25
#define G1 26
#define BL1 27
#define R2 14
#define G2 12
#define BL2 13
#define CH_A 23
#define CH_B 19
#define CH_C 5
#define CH_D 17
#define CH_E -1 // assign to any available pin if using panels with 1/32 scan
#define CLK 16
#define LAT 4
#define OE 15
*/

#if defined(_VARIANT_MATRIXPORTAL_M4_) // MatrixPortal M4
  #define BUTTON_UP 2
  #define BUTTON_DOWN 3
#elif defined(ARDUINO_ADAFRUIT_MATRIXPORTAL_ESP32S3) // MatrixPortal ESP32-S3
  #define R1_PIN 42
  #define G1_PIN 41
  #define B1_PIN 40
  #define R2_PIN 38
  #define G2_PIN 39
  #define B2_PIN 37
  #define A_PIN  45
  #define B_PIN  36
  #define C_PIN  48
  #define D_PIN  35
  #define E_PIN  21 
  #define LAT_PIN 47
  #define OE_PIN  14
  #define CLK_PIN 2
  // Button pins
  #define BUTTON_UP 6
  #define BUTTON_DOWN 7
  //#include <Adafruit_LIS3DH.h>      // Library for built-in For accelerometer
  #define PIN_E 21

#elif defined(__SAMD51__) // M4 Metro Variants (Express, AirLift)
  //
#elif defined(_SAMD21_) // Feather M0 variants
  //
#elif defined(NRF52_SERIES) // Special nRF52840 FeatherWing pinout
  //
#elif USB_VID == 0x239A && USB_PID == 0x8113 // Feather ESP32-S3 No PSRAM
  // M0/M4/RP2040 Matrix FeatherWing compatible:
#elif USB_VID == 0x239A && USB_PID == 0x80EB // Feather ESP32-S2
  // M0/M4/RP2040 Matrix FeatherWing compatible:
#elif defined(ESP32)
  // 'Safe' pins, not overlapping any peripherals:
  // GPIO.out: 4, 12, 13, 14, 15, 21, 27, GPIO.out1: 32, 33
  // Peripheral-overlapping pins, sorted from 'most expendible':
  // 16, 17 (RX, TX)
  // 25, 26 (A0, A1)
  // 18, 5, 9 (MOSI, SCK, MISO)
  // 22, 23 (SCL, SDA)
#elif defined(ARDUINO_TEENSY40)
  //
#elif defined(ARDUINO_TEENSY41)
  //
#elif defined(ARDUINO_ADAFRUIT_FEATHER_RP2040)
  // RP2040 support requires the Earle Philhower board support package;
  // will not compile with the Arduino Mbed OS board package.
  // The following pinout works with the Adafruit Feather RP2040 and
  // original RGB Matrix FeatherWing (M0/M4/RP2040, not nRF version).
  // Pin numbers here are GP## numbers, which may be different than
  // the pins printed on some boards' top silkscreen.
#endif


// Configure for your panel(s) as appropriate!
#define PANEL_WIDTH 128
#define PANEL_HEIGHT 32         // Panel height of 64 will required PIN_E to be defined.

#ifdef VIRTUAL_PANE
 #define PANELS_NUMBER 4         // Number of chained panels, if just a single panel, obviously set to 1
#else
 #define PANELS_NUMBER 2         // Number of chained panels, if just a single panel, obviously set to 1
#endif

#define PANE_WIDTH PANEL_WIDTH * PANELS_NUMBER
#define PANE_HEIGHT PANEL_HEIGHT
#define NUM_LEDS PANE_WIDTH*PANE_HEIGHT

#ifdef VIRTUAL_PANE
 #define NUM_ROWS 2 // Number of rows of chained INDIVIDUAL PANELS
 #define NUM_COLS 2 // Number of INDIVIDUAL PANELS per ROW
 #define PANEL_CHAIN NUM_ROWS*NUM_COLS    // total number of panels chained one to another
 // Change this to your needs, for details on VirtualPanel pls read the PDF!
 #define SERPENT true
 #define TOPDOWN false
#endif


#ifdef VIRTUAL_PANE
VirtualMatrixPanel *matrix = nullptr;
MatrixPanel_I2S_DMA *chain = nullptr;
#else
MatrixPanel_I2S_DMA *dma_display = nullptr;
#endif

// patten change delay
#define PATTERN_DELAY 2000

uint16_t time_counter = 0, cycles = 0, fps = 0;
unsigned long fps_timer;

// gradient buffer
CRGB *ledbuff;
//

unsigned long t1, t2, s1=0, s2=0, s3=0;
uint32_t ccount1, ccount2;

uint8_t color1 = 0, color2 = 0, color3 = 0;
uint16_t x,y;

const char *str = "* ESP32 I2S DMA *";

// LumiFur Global Variables ---------------------------------------------------

// View switching
int currentView = 4; // Current & initial view being displayed
const int totalViews = 12; // Total number of views to cycle through

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

// Variables for plasma effect
//uint16_t time_counter = 0, cycles = 0, fps = 0;
//unsigned long fps_timer;

CRGB currentColor;
CRGBPalette16 palettes[] = {LavaColors_p, HeatColors_p, RainbowColors_p, RainbowStripeColors_p, CloudColors_p};
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

// Bitmaps --------------------------------------------------------------------

// 'Apple_logo_black', 18x21px
static char appleLogoApple_logo_black[] = {
	0x00, 0x18, 0x00, 0x00, 0x30, 0x00, 0x00, 0x70, 0x00, 0x00, 0xe0, 0x00, 0x00, 0xc0, 0x00, 0x1e, 
	0x1e, 0x00, 0x7f, 0xff, 0x80, 0x7f, 0xff, 0x80, 0x7f, 0xfe, 0x00, 0xff, 0xfe, 0x00, 0xff, 0xfe, 
	0x00, 0xff, 0xfe, 0x00, 0xff, 0xfe, 0x00, 0xff, 0xfe, 0x00, 0xff, 0xff, 0x00, 0x7f, 0xff, 0x80, 
	0x7f, 0xff, 0x80, 0x3f, 0xff, 0x80, 0x3f, 0xff, 0x00, 0x1f, 0xfe, 0x00, 0x0f, 0x3c, 0x00
};

/////////////////////////// Facial icons 

// Right side of the helmet
static char nose[] = {
    B00000000,
              B01111110,
              B00111111,
              B00000011,
              B00000011,
              B00000001,
              B00000000,
              B00000000
              };
static char maw[] = {
	0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x3f, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0xe3, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe3, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0xe0, 0x3f, 0x80, 0x00, 0x00, 0x00, 0xf0, 0x00, 0xe0, 0x3f, 0x80, 0x00, 0x00, 0x00, 0xf0, 0x00, 
	0xff, 0xff, 0xf0, 0x00, 0x00, 0x1f, 0xff, 0x00, 0xff, 0xff, 0xf0, 0x00, 0x00, 0x1f, 0xff, 0x00, 
	0x00, 0x00, 0x7f, 0x00, 0x00, 0xfc, 0x07, 0xe0, 0x00, 0x00, 0x7f, 0x00, 0x00, 0xfc, 0x07, 0xe0, 
	0x00, 0x00, 0x07, 0xf0, 0x0f, 0xe0, 0x00, 0xf8, 0x00, 0x00, 0x07, 0xf0, 0x0f, 0xe0, 0x00, 0xf8, 
	0x00, 0x00, 0x00, 0x3f, 0xfc, 0x00, 0x00, 0x1f, 0x00, 0x00, 0x00, 0x3f, 0xfc, 0x00, 0x00, 0x1f
             };
static char Glitch1[] = {
    B00110000, B00010000, B00000100, B00000100,
                 B00000000, B00101000, B01100000, B00111010,
                 B00110001, B00101000, B00000001, B00010011,
                 B00000111, B00100010, B00100001, B10100001,
                 B01011101, B11110000, B00000011, B11111111,
                 B00111000, B01101001, B01001110, B00000100,
                 B10100000, B01001110, B01001100, B00000110,
                 B10000001, B00000111, B11100100, B00000000
                 };
static char Glitch2[] = {
    B00000000, B00000000, B00000000, B00000100,
                 B00000000, B00000000, B00000000, B00011110,
                 B00100000, B00010000, B00000000, B01001011,
                 B00000111, B00100000, B00000001, B11100111,
                 B00011111, B11110000, B00000110, B11111111,
                 B00001011, B11111101, B01111100, B00000000,
                 B11100110, B00010110, B01011000, B00000000,
                 B00000000, B00000111, B11100000, B00000000
                 };
static char Eye[] = {
	0x00, 0xff, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x0f, 0xff, 0xfc, 0x00, 0x0f, 0xff, 0xfc, 0x00, 
	0x3f, 0xff, 0xff, 0xc0, 0x3f, 0xff, 0xff, 0xc0, 0xff, 0xff, 0xff, 0xfc, 0xff, 0xff, 0xff, 0xfc, 
	0xff, 0x00, 0x00, 0x3f, 0xff, 0x00, 0x00, 0x3f, 0x3c, 0x00, 0x00, 0x03, 0x3c, 0x00, 0x00, 0x03, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
             };
static char Angry[] = {
        B00000000, B00000000,
               B00011111, B11111100,
               B00111111, B11111110,
               B00111111, B11111100,
               B00011111, B11111000,
               B00001111, B11100000,
               B00000011, B10000000,
               B00000000, B00000000
               };
static char Spooked[] = {
    B00000011, B11000000,
                 B00000111, B11100000,
                 B00001111, B11110000,
                 B00001111, B11110000,
                 B00001111, B11110000,
                 B00001111, B11110000,
                 B00000111, B11100000,
                 B00000011, B11000000
                 };
static char vwv[] = {
        B00001110, B00000000,
             B00000111, B10000000,
             B00000001, B11100000,
             B00000000, B01111000,
             B00000000, B01111000,
             B00000001, B11100000,
             B00000111, B10000000,
             B00001110, B00000000};
static char blush[] = {
	0x00, 0xc0, 0x00, 0xe0, 0x01, 0xc0, 0x03, 0x80, 0x07, 0x80, 0x07, 0x00, 0x0e, 0x00, 0x1c, 0x00, 
	0x3c, 0x00, 0x38, 0x00, 0x70, 0x00, 0xe0, 0x00, 0x60, 0x00
};
static char semicircleeyes[] = {
  	0xff, 0xff, 0xff, 0xfe, 0x7f, 0xff, 0xff, 0xfc, 0x7f, 0xff, 0xff, 0xfc, 0x7f, 0xff, 0xff, 0xfc, 
	0x3f, 0xff, 0xff, 0xf8, 0x3f, 0xff, 0xff, 0xf8, 0x1f, 0xff, 0xff, 0xf0, 0x0f, 0xff, 0xff, 0xe0, 
	0x07, 0xff, 0xff, 0xc0, 0x01, 0xff, 0xff, 0x00, 0x00, 0x7f, 0xfc, 0x00, 0x00, 0x07, 0xc0, 0x00
};
static char x_eyes[] = {
	  0xe0, 0x00, 0x00, 0x0e, 0xf8, 0x00, 0x00, 0x3e, 0x3e, 0x00, 0x00, 0xf8, 0x0f, 0xc0, 0x07, 0xe0, 
	0x03, 0xf0, 0x1f, 0x80, 0x00, 0x7c, 0x7c, 0x00, 0x00, 0x1f, 0xf0, 0x00, 0x00, 0x07, 0xc0, 0x00, 
	0x00, 0x1f, 0xf0, 0x00, 0x00, 0x7c, 0x7c, 0x00, 0x03, 0xf0, 0x1f, 0x80, 0x0f, 0xc0, 0x07, 0xe0, 
	0x3e, 0x00, 0x00, 0xf8, 0xf8, 0x00, 0x00, 0x3e, 0xe0, 0x00, 0x00, 0x0e
};
static char slanteyes[] = {
0x80, 0x00, 0x00, 0xf8, 0x00, 0x00, 0xff, 0x80, 0x00, 0xff, 0xf0, 0x00, 0xff, 0xff, 0x00, 0xff, 
	0xff, 0xf0, 0xff, 0xff, 0xff, 0x7f, 0xff, 0xff, 0x7f, 0xff, 0xff, 0x7f, 0xff, 0xff, 0x3f, 0xff, 
	0xfe, 0x1f, 0xff, 0xfc, 0x0f, 0xff, 0xf8, 0x07, 0xff, 0xf0, 0x03, 0xff, 0xe0, 0x00, 0x7f, 0x00
};
static char spiral[] = {
0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x0e, 0x00, 0x00, 0x00, 0xff, 0xc0, 0x00, 
	0x03, 0xff, 0xf0, 0x00, 0x07, 0xf1, 0xf8, 0x00, 0x0f, 0x80, 0x3c, 0x00, 0x1e, 0x00, 0x1e, 0x00, 
	0x3c, 0x00, 0x0f, 0x00, 0x38, 0x0e, 0x07, 0x00, 0x78, 0x3f, 0x83, 0x80, 0x70, 0x7f, 0xc3, 0x80, 
	0x70, 0xf1, 0xc3, 0x80, 0xe1, 0xe1, 0xc3, 0x80, 0xe1, 0xc7, 0xc3, 0x80, 0xe1, 0xc7, 0xc3, 0x80, 
	0xe1, 0xc7, 0xc3, 0x80, 0xe1, 0xc1, 0x07, 0x80, 0xe1, 0xe0, 0x07, 0x00, 0xf0, 0xe0, 0x0f, 0x00, 
	0x60, 0xf8, 0x3e, 0x00, 0x00, 0x7f, 0xfc, 0x00, 0x00, 0x3f, 0xf8, 0x00, 0x00, 0x0f, 0xe0, 0x00, 
	0x00, 0x00, 0x00, 0x00
  };
// Left side of the helmet
static char noseL[] = {
    B00000000,
               B01111110,
               B11111100,
               B11000000,
               B11000000,
               B10000000,
               B00000000,
               B00000000
               };
static char mawL[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0xfc, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 0xc7, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 0xc7, 
	0x00, 0x0f, 0x00, 0x00, 0x00, 0x01, 0xfc, 0x07, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x01, 0xfc, 0x07, 
	0x00, 0xff, 0xf8, 0x00, 0x00, 0x0f, 0xff, 0xff, 0x00, 0xff, 0xf8, 0x00, 0x00, 0x0f, 0xff, 0xff, 
	0x07, 0xe0, 0x3f, 0x00, 0x00, 0xfe, 0x00, 0x00, 0x07, 0xe0, 0x3f, 0x00, 0x00, 0xfe, 0x00, 0x00, 
	0x1f, 0x00, 0x07, 0xf0, 0x0f, 0xe0, 0x00, 0x00, 0x1f, 0x00, 0x07, 0xf0, 0x0f, 0xe0, 0x00, 0x00, 
	0xf8, 0x00, 0x00, 0x3f, 0xfc, 0x00, 0x00, 0x00, 0xf8, 0x00, 0x00, 0x3f, 0xfc, 0x00, 0x00, 0x00
              };
static char Glitch1L[] = {
        B00000000, B00000000, B00000000, B00000100,
                  B00000000, B00000000, B00000000, B00011110,
                  B00100000, B00010000, B00000000, B01001011,
                  B00000111, B00100000, B00000001, B11100111,
                  B00011111, B11110000, B00000110, B11111111,
                  B00001011, B11111101, B01111100, B00000000,
                  B11100110, B00010110, B01011000, B00000000,
                  B00000000, B00000111, B11100000, B00000000
                  };
static char Glitch2L[] = {
    0x0c, 0x08, 0x20, 0x20, 0x00, 0x14, 0x06, 0x5c, 0x0c, 0x14, 0x80, 0xc8, 0xe0,
                  0x22, 0xc2, 0xc2, 0xba, 0x0f, 0xc0, 0xff, 0x1c, 0x96, 0x72, 0x20, 0x05, 0x72,
                  0x32, 0x60, 0x81, 0xe0, 0x27, 0x00
                  };
static char EyeL[] = {
	0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x3f, 0xff, 0xf0, 0x00, 0x3f, 0xff, 0xf0, 
	0x03, 0xff, 0xff, 0xfc, 0x03, 0xff, 0xff, 0xfc, 0x3f, 0xff, 0xff, 0xff, 0x3f, 0xff, 0xff, 0xff, 
	0xfc, 0x00, 0x00, 0xff, 0xfc, 0x00, 0x00, 0xff, 0xc0, 0x00, 0x00, 0x3c, 0xc0, 0x00, 0x00, 0x3c, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
              };
static char AngryL[] = {
    B00000000, B00000000,
                B00111111, B11111000,
                B01111111, B11111100,
                B00111111, B11111100,
                B00011111, B11111000,
                B00000111, B11110000,
                B00000001, B11000000,
                B00000000, B00000000
                };
static char SpookedL[] = {
    B00000011, B11000000,
                  B00000111, B11100000,
                  B00001111, B11110000,
                  B00001111, B11110000,
                  B00001111, B11110000,
                  B00001111, B11110000,
                  B00000111, B11100000,
                  B00000011, B11000000
                  };
static char vwvL[] = {
    B00000000, B01110000,
              B00000001, B11100000,
              B00000111, B10000000,
              B00011110, B00000000,
              B00011110, B00000000,
              B00000111, B10000000,
              B00000001, B11100000,
              B00000000, B01110000
              };
static char blushL[] = {
	0x60, 0x00, 0xe0, 0x00, 0x70, 0x00, 0x38, 0x00, 0x3c, 0x00, 0x1c, 0x00, 0x0e, 0x00, 0x07, 0x00, 
	0x07, 0x80, 0x03, 0x80, 0x01, 0xc0, 0x00, 0xe0, 0x00, 0xc0
  };
static char slanteyesL[] = {
0x00, 0x00, 0x01, 0x00, 0x00, 0x1f, 0x00, 0x01, 0xff, 0x00, 0x0f, 0xff, 0x00, 0xff, 0xff, 0x0f, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xfe, 0x7f, 0xff, 
	0xfc, 0x3f, 0xff, 0xf8, 0x1f, 0xff, 0xf0, 0x0f, 0xff, 0xe0, 0x07, 0xff, 0xc0, 0x00, 0xfe, 0x00
  };
static char spiralL[] = {
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


// Bitmap Drawing Functions ------------------------------------------------
void drawXbm565(int x, int y, int width, int height, const char *xbm, uint16_t color = dma_display->color565(255, 255, 255)) 
{
  if (width % 8 != 0) {
      width =  ((width / 8) + 1) * 8;
  }
    for (int i = 0; i < width * height / 8; i++ ) {
      unsigned char charColumn = pgm_read_byte(xbm + i);
      for (int j = 0; j < 8; j++) {
        int targetX = (i * 8 + j) % width + x;
        int targetY = (8 * i / (width)) + y;
        if (bitRead(charColumn, j)) {
          uint8_t r = (color >> 16) & 0xFF;
          uint8_t g = (color >> 8) & 0xFF;
          uint8_t b = color & 0xFF;
          dma_display->drawPixelRGB888(targetX, targetY, r, g, b);
        }
      }
    }
}

//Array of all the bitmaps (Total bytes used to store images in PROGMEM = Cant remember)
// Create code to list available bitmaps for future bluetooth control

int current_view = 4;
static int number_of_views = 12;

static char view_name[13][30] = {
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
"slanteyes",
"spiral"
};

static char *view_bits[13] = {
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
slanteyes,
spiral
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
  for(int i=1;;i++) {                     // Loop forever...
    digitalWrite(LED_BUILTIN, i & 1); // LED on/off blink to alert user
    delay(x);
  }
}

void displayLoadingBar() {
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
  dma_display->drawRect(barX + 1, (barY * 2) + 1, progressWidth, barHeight - 2, dma_display->color565(255, 255, 255));
  dma_display->fillRect((barX + 1) + 64, (barY * 2) + 1, progressWidth, barHeight - 2, dma_display->color565(255, 255, 255));

/*
  // Display percentage text
  char progressText[16];
  sprintf(progressText, "%d%%", loadingProgress);
  matrix.setTextColor(dma_display->color565(255, 255, 255));
  matrix.setCursor(barX - 8, barY + 14); // Position above the bar
  matrix.print(progressText);
*/
  ////matrix.show(); // Update the matrix display
  
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
      blinkProgress = 100 * easeInOutQuad(normalizedProgress); // Using ease-in-out for natural acceleration and deceleration
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
        if (newX >= 0 && newX < MATRIX_WIDTH && newY >= 0 && newY < MATRIX_HEIGHT) {
          dma_display->drawPixel(newX, newY, color);
        }
      }
    }
  }
}

void updateRotatingSpiral() {
  static unsigned long lastUpdate = 0;
  static float currentAngle = 0;
  //static uint16_t time_counter = 0;
  unsigned long currentTime = millis();
  if (currentTime - lastUpdate > 20) { // Update every 100 ms
    lastUpdate = currentTime;

    // Increment the angle for rotation
    currentAngle += 10.0;
    if (currentAngle >= 360.0) {
      currentAngle -= 360.0; // Wrap angle to 0-359°
    }

  // Increment time_counter to change colors over time
    time_counter++;


// Precompute values for efficiency
    int16_t v = 128;
    uint8_t wibble = sin8(time_counter);
    v += sin16(wibble * 3 + time_counter);
    v += cos16((128 - wibble) + time_counter);
    v += sin16(cos8(-time_counter) / 8);
// Map the calculated value to a color using the palette
CRGB currentColor = ColorFromPalette(currentPalette, (v >> 8) & 0xFF); // Ensure within palette range
    uint16_t color = dma_display->color565(currentColor.r, currentColor.g, currentColor.b);

    // Draw the rotating bitmaps
    drawRotatedBitmap(26, 10, spiral, 25, 25, currentAngle, color);
    drawRotatedBitmap(97, 10, spiralL, 25, 25, -currentAngle, color);
}
}


// Draw the blinking eyes
void blinkingEyes() {

  // Draw  eyes
  if (currentView == 4 || currentView == 5) {
  drawXbm565(0, 0, 32, 16, Eye, dma_display->color565(255, 255, 255));
  drawXbm565(96, 0, 32, 16, EyeL, dma_display->color565(255, 255, 255));
  }
  if (currentView == 6) {
    drawXbm565(0, 0, 32, 12, semicircleeyes, dma_display->color565(255, 255, 255));
    drawXbm565(96, 0, 32, 12, semicircleeyes, dma_display->color565(255, 255, 255));
  }
  if (currentView == 7) {
    drawXbm565(0, 0, 31, 15, x_eyes, dma_display->color565(255, 255, 255));
    drawXbm565(96, 0, 31, 15, x_eyes, dma_display->color565(255, 255, 255));
  }
  /*
  if (currentView == 8) {
    drawXbm565(0, 0, Angry, 16, 8, dma_display->color565(255, 255, 255));
    drawXbm565(96, 0, AngryL, 16, 8, dma_display->color565(255, 255, 255));
  }
  if (currentView == 9) {
    drawXbm565(0, 0, Spooked, 16, 8, dma_display->color565(255, 255, 255));
    drawXbm565(96, 0, SpookedL, 16, 8, dma_display->color565(255, 255, 255));
  }
  */
  if (currentView == 8) {
    drawXbm565(0, 0, 24, 16, slanteyes, dma_display->color565(255, 255, 255));
    drawXbm565(104, 0, 24, 16, slanteyes, dma_display->color565(255, 255, 255));
  }
  if (currentView == 9) {
  //  drawXbm565(15, 0, 25, 25, spiral, dma_display->color565(255, 255, 255));
  //  drawXbm565(88, 0, 25, 25, spiralL, dma_display->color565(255, 255, 255));
  }

  if (isBlinking) {
    // Calculate the height of the black box
    int boxHeight = map(blinkProgress, 0, 100, 0, 16); // From 0 to 16 pixels

    // Draw black boxes over the eyes
    dma_display->drawRect(0, 0, 32, boxHeight, 0);     // Cover the right eye
    dma_display->drawRect(96, 0, 32, boxHeight, 0);    // Cover the left eye
  }

  ////matrix.show(); // Update the display
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
  uint16_t blushColor = dma_display->color565(blushBrightness, 0, blushBrightness);

     drawXbm565(35, 12, 11, 13, blush, blushColor);
     drawXbm565(30, 12, 11, 13, blush, blushColor);
     drawXbm565(25, 12, 11, 13, blush, blushColor);
     drawXbm565(82, 12, 11, 13, blushL, blushColor);
     drawXbm565(87, 12, 11, 13, blushL, blushColor);
     drawXbm565(92, 12, 11, 13, blushL, blushColor);
}

void drawTransFlag() {
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
   drawXbm565(56, 10, 8, 8, nose, dma_display->color565(255, 255, 255));
   drawXbm565(64, 10, 8, 8, noseL, dma_display->color565(255, 255, 255));
   drawXbm565(0, 16, 64, 16, maw, dma_display->color565(255, 255, 255));
   drawXbm565(64, 16, 64, 16, mawL, dma_display->color565(255, 255, 255));
   //drawXbm565(0, 0, Eye, 32, 16, dma_display->color565(255, 255, 255));
   //drawXbm565(96, 0, EyeL, 32, 16, dma_display->color565(255, 255, 255));

    /*
     drawXbm565(35, 12, blush, 11, 13, dma_display->color565(255, 0, 255));
     drawXbm565(30, 12, blush, 11, 13, dma_display->color565(255, 0, 255));
     drawXbm565(25, 12, blush, 11, 13, dma_display->color565(255, 0, 255));
     drawXbm565(82, 12, blushL, 11, 13, dma_display->color565(255, 0, 255));
     drawXbm565(87, 12, blushL, 11, 13, dma_display->color565(255, 0, 255));
     drawXbm565(92, 12, blushL, 11, 13, dma_display->color565(255, 0, 255));
     */

   // Draw the blush (with fading effect in view 2)
   if (currentView == 2) {
   drawBlush();
}
   
   // Draw blinking eyes
   blinkingEyes();

   ////matrix.show(); // Update the matrix display
}

void patternPlasma() {
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

  // Refresh display
  //matrix.show();
}





void setup() {

  Serial.begin(BAUD_RATE);
  Serial.println("Starting LumiFur...");

  // redefine pins if required
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
  dma_display->setBrightness8(100);
#else
  chain = new MatrixPanel_I2S_DMA(mxconfig);
  chain->begin();
  chain->setBrightness8(255);
  // create VirtualDisplay object based on our newly created dma_display object
  matrix = new VirtualMatrixPanel((*chain), NUM_ROWS, NUM_COLS, PANEL_WIDTH, PANEL_HEIGHT, CHAIN_TOP_LEFT_DOWN);
#endif

  ledbuff = (CRGB *)malloc(NUM_LEDS * sizeof(CRGB));  // allocate buffer for some tests
  buffclear(ledbuff);

// Initialize accelerometer if MATRIXPORTAL_ESP32S3 is used
#if defined (ARDUINO_ADAFRUIT_MATRIXPORTAL_ESP32S3)
  //Adafruit_LIS3DH accel = Adafruit_LIS3DH();
#endif

}

uint8_t wheelval = 0;



void loop(){

  Serial.printf("Cycle: %d\n", ++cycles);

#ifndef NO_GFX
  drawText(wheelval++);
#endif

  Serial.print("Estimating clearScreen() - ");
  ccount1 = XTHAL_GET_CCOUNT();
  dma_display->clearScreen();
  ccount1 = XTHAL_GET_CCOUNT() - ccount1;
  Serial.printf("%d ticks\n", ccount1);
  delay(PATTERN_DELAY);

/*
// Power supply tester
// slowly fills matrix with white, stressing PSU
  for (int y=0; y!=PANE_HEIGHT; ++y){
    for (int x=0; x!=PANE_WIDTH; ++x){
      matrix->drawPixelRGB888(x, y, 255,255,255);
      //matrix->drawPixelRGB888(x, y-1, 255,0,0);       // pls, be gentle :)
      delay(10);
    }
  }
  delay(5000);
*/



/*
#ifndef VIRTUAL_PANE
  // simple solid colors
  Serial.println("Fill screen: RED");
  matrix->fillScreenRGB888(255, 0, 0);
  delay(PATTERN_DELAY);
  Serial.println("Fill screen: GREEN");
  matrix->fillScreenRGB888(0, 255, 0);
  delay(PATTERN_DELAY);
  Serial.println("Fill screen: BLUE");
  matrix->fillScreenRGB888(0, 0, 255);
  delay(PATTERN_DELAY);
#endif

  for (uint8_t i=5; i; --i){
    Serial.print("Estimating single drawPixelRGB888(r, g, b) ticks: ");
    color1 = random8();
    ccount1 = XTHAL_GET_CCOUNT();
    matrix->drawPixelRGB888(i,i, color1, color1, color1);
    ccount1 = XTHAL_GET_CCOUNT() - ccount1;
    Serial.printf("%d ticks\n", ccount1);
  }

// Clearing CRGB ledbuff
  Serial.print("Estimating ledbuff clear time: ");
  t1 = micros();
  ccount1 = XTHAL_GET_CCOUNT();
  buffclear(ledbuff);
  ccount1 = XTHAL_GET_CCOUNT() - ccount1;
  t2 = micros()-t1;
  Serial.printf("%lu us, %u ticks\n\n", t2, ccount1);

#ifndef VIRTUAL_PANE
  // Bare fillscreen(r, g, b)
  Serial.print("Estimating fillscreenRGB888(r, g, b) time: ");
  t1 = micros();
  ccount1 = XTHAL_GET_CCOUNT();
  matrix->fillScreenRGB888(64, 64, 64);   // white
  ccount2 = XTHAL_GET_CCOUNT() - ccount1;
  t2 = micros()-t1;
  s1+=t2;
  Serial.printf("%lu us, avg: %lu, ccnt: %d\n", t2, s1/cycles, ccount2);
  delay(PATTERN_DELAY);
#endif

  Serial.print("Estimating full-screen fillrate with looped drawPixelRGB888(): ");
  y = PANE_HEIGHT;
  t1 = micros();
  ccount1 = XTHAL_GET_CCOUNT();
  do {
    --y;
    uint16_t x = PANE_WIDTH;
    do {
      --x;
        matrix->drawPixelRGB888( x, y, 0, 0, 0);
    } while(x);
  } while(y);
  ccount1 = XTHAL_GET_CCOUNT() - ccount1;
  t2 = micros()-t1;
  Serial.printf("%lu us, %u ticks\n", t2, ccount1);



// created random color gradient in ledbuff
  uint8_t color1 = 0;
  uint8_t color2 = random8();
  uint8_t color3 = 0;

  for (uint16_t i = 0; i<NUM_LEDS; ++i){
    ledbuff[i].r=color1++;
    ledbuff[i].g=color2;
    if (i%PANE_WIDTH==0)
      color3+=255/PANE_HEIGHT;

    ledbuff[i].b=color3;
  }
//

//
  Serial.print("Estimating ledbuff-to-matrix fillrate with drawPixelRGB888(), time: ");
  t1 = micros();
  ccount1 = XTHAL_GET_CCOUNT();
  mxfill(ledbuff);
  ccount1 = XTHAL_GET_CCOUNT() - ccount1;
  t2 = micros()-t1;
  s2+=t2;
  Serial.printf("%lu us, avg: %lu, %d ticks:\n", t2, s2/cycles, ccount1);
  delay(PATTERN_DELAY);
//

#ifndef NO_FAST_FUNCTIONS
  // Fillrate for fillRect() function
  Serial.print("Estimating fullscreen fillrate with fillRect() time: ");
  t1 = micros();
  matrix->fillRect(0, 0, PANE_WIDTH, PANE_HEIGHT, 0, 224, 0);
  t2 = micros()-t1;
  Serial.printf("%lu us\n", t2);
  delay(PATTERN_DELAY);


  Serial.print("Chessboard with fillRect(): ");  // шахматка
  matrix->fillScreen(0);
  x =0, y = 0;
  color1 = random8();
  color2 = random8();
  color3 = random8();
  bool toggle=0;
  t1 = micros();
  do {
    do{
      matrix->fillRect(x, y, 8, 8, color1, color2, color3);
      x+=16;
    }while(x < PANE_WIDTH);
    y+=8;
    toggle = !toggle;
    x = toggle ? 8 : 0;
  }while(y < PANE_HEIGHT);
  t2 = micros()-t1;
  Serial.printf("%lu us\n", t2);
  delay(PATTERN_DELAY);
#endif

// ======== V-Lines ==========
  Serial.println("Estimating V-lines with drawPixelRGB888(): ");  //
  matrix->fillScreen(0);
  color1 = random8();
  color2 = random8();
  x = y = 0;
  t1 = micros();
  ccount1 = XTHAL_GET_CCOUNT();
  do {
    y=0;
    do{
      matrix->drawPixelRGB888(x, y, color1, color2, color3);
    } while(++y != PANE_HEIGHT);
    x+=2;
  } while(x != PANE_WIDTH);
  ccount1 = XTHAL_GET_CCOUNT() - ccount1;
  t2 = micros()-t1;
  Serial.printf("%lu us, %u ticks\n", t2, ccount1);
  delay(PATTERN_DELAY);

#ifndef NO_FAST_FUNCTIONS
  Serial.println("Estimating V-lines with vlineDMA(): ");  //
  matrix->fillScreen(0);
  color2 = random8();
  x = y = 0;
  t1 = micros();
  ccount1 = XTHAL_GET_CCOUNT();
  do {
    matrix->drawFastVLine(x, y, PANE_HEIGHT, color1, color2, color3);
    x+=2;
  } while(x != PANE_WIDTH);
  ccount1 = XTHAL_GET_CCOUNT() - ccount1;
  t2 = micros()-t1;
  Serial.printf("%lu us, %u ticks\n", t2, ccount1);
  delay(PATTERN_DELAY);

  Serial.println("Estimating V-lines with fillRect(): ");  //
  matrix->fillScreen(0);
  color1 = random8();
  color2 = random8();
  x = y = 0;
  t1 = micros();
  ccount1 = XTHAL_GET_CCOUNT();
  do {
    matrix->fillRect(x, y, 1, PANE_HEIGHT, color1, color2, color3);
    x+=2;
  } while(x != PANE_WIDTH);
  ccount1 = XTHAL_GET_CCOUNT() - ccount1;
  t2 = micros()-t1;
  Serial.printf("%lu us, %u ticks\n", t2, ccount1);
  delay(PATTERN_DELAY);
#endif



// ======== H-Lines ==========
  Serial.println("Estimating H-lines with drawPixelRGB888(): ");  //
  matrix->fillScreen(0);
  color2 = random8();
  x = y = 0;
  t1 = micros();
  ccount1 = XTHAL_GET_CCOUNT();
  do {
    x=0;
    do{
      matrix->drawPixelRGB888(x, y, color1, color2, color3);
    } while(++x != PANE_WIDTH);
    y+=2;
  } while(y != PANE_HEIGHT);
  ccount1 = XTHAL_GET_CCOUNT() - ccount1;
  t2 = micros()-t1;
  Serial.printf("%lu us, %u ticks\n", t2, ccount1);
  delay(PATTERN_DELAY);

#ifndef NO_FAST_FUNCTIONS
  Serial.println("Estimating H-lines with hlineDMA(): ");
  matrix->fillScreen(0);
  color2 = random8();
  color3 = random8();
  x = y = 0;
  t1 = micros();
  ccount1 = XTHAL_GET_CCOUNT();
  do {
    matrix->drawFastHLine(x, y, PANE_WIDTH, color1, color2, color3);
    y+=2;
  } while(y != PANE_HEIGHT);
  ccount1 = XTHAL_GET_CCOUNT() - ccount1;
  t2 = micros()-t1;
  Serial.printf("%lu us, %u ticks\n", t2, ccount1);
  delay(PATTERN_DELAY);

  Serial.println("Estimating H-lines with fillRect(): ");  //
  matrix->fillScreen(0);
  color2 = random8();
  color3 = random8();
  x = y = 0;
  t1 = micros();
  ccount1 = XTHAL_GET_CCOUNT();
  do {
    matrix->fillRect(x, y, PANE_WIDTH, 1, color1, color2, color3);
    y+=2;
  } while(y != PANE_HEIGHT);
  ccount1 = XTHAL_GET_CCOUNT() - ccount1;
  t2 = micros()-t1;
  Serial.printf("%lu us, %u ticks\n", t2, ccount1);
  delay(PATTERN_DELAY);
#endif




  Serial.println("\n====\n");

  // take a rest for a while
  delay(10000);
}

*/
}



void buffclear(CRGB *buf){
  memset(buf, 0x00, NUM_LEDS * sizeof(CRGB)); // flush buffer to black  
}

void IRAM_ATTR mxfill(CRGB *leds){
  uint16_t y = PANE_HEIGHT;
  do {
    --y;
    uint16_t x = PANE_WIDTH;
    do {
      --x;
        uint16_t _pixel = y * PANE_WIDTH + x;
        dma_display->drawPixelRGB888( x, y, leds[_pixel].r, leds[_pixel].g, leds[_pixel].b);
    } while(x);
  } while(y);
}
//

/**
 *  The one for 256+ matrices
 *  otherwise this:
 *    for (uint8_t i = 0; i < MATRIX_WIDTH; i++) {}
 *  turns into an infinite loop
 */
uint16_t XY16( uint16_t x, uint16_t y)
{ 
  if (x<PANE_WIDTH && y < PANE_HEIGHT){
    return (y * PANE_WIDTH) + x;
  } else {
    return 0;
  }
}


#ifdef NO_GFX
void drawText(int colorWheelOffset){}
#else
void drawText(int colorWheelOffset){
  // draw some text
  dma_display->setTextSize(1);     // size 1 == 8 pixels high
  dma_display->setTextWrap(false); // Don't wrap at end of line - will do ourselves

  dma_display->setCursor(5, 5);    // start at top left, with 5,5 pixel of spacing
  uint8_t w = 0;

  for (w=0; w<strlen(str); w++) {
    dma_display->setTextColor(colorWheel((w*32)+colorWheelOffset));
    dma_display->print(str[w]);
  }
}
#endif


uint16_t colorWheel(uint8_t pos) {
  if(pos < 85) {
    return dma_display->color565(pos * 3, 255 - pos * 3, 0);
  } else if(pos < 170) {
    pos -= 85;
    return dma_display->color565(255 - pos * 3, 0, pos * 3);
  } else {
    pos -= 170;
    return dma_display->color565(0, pos * 3, 255 - pos * 3);
  }
}
