/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
// This file is used to define the pinout for the 
//different boards that are supported by the library.
/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
/////////////////////////////////////////////////////

// HUB75E port pinout
// R1 | G1
// B1 | GND
// R2 | G2
// B2 | E
//  A | B
//  C | D
// CLK| LAT
// OE | GND

/*  
// Default library pin configuration for the reference
// you can redefine only ones you need later on object creation
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
  #define PIN_E 21 // E pin for 64px high panels
  // Button pins
  #define BUTTON_UP 6
  #define BUTTON_DOWN 7
  #include <Wire.h>                 // For I2C sensors
  //#include <SPI.h>                  // For SPI sensors
  #include "Adafruit_APDS9960.h"    // Library for built-in gesture sensor
  //the pin that the interrupt is attached to
  #define INT_PIN 3
  Adafruit_APDS9960 apds;
  #include <Adafruit_LIS3DH.h>      // Library for built-in For accelerometer
  #include <Adafruit_NeoPixel.h>    // Library for built-in NeoPixel
  #define STATUS_LED_PIN 4
  Adafruit_NeoPixel statusPixel(1, STATUS_LED_PIN, NEO_GRB + NEO_KHZ800);
#elif defined(ARDUINO_ADAFRUIT_METRO_ESP32S3) //Metro ESP32-S3
  #define R1_PIN 2
  #define G1_PIN 3
  #define B1_PIN 4
  #define R2_PIN 5
  #define G2_PIN 6
  #define B2_PIN 7
  #define A_PIN  A0
  #define B_PIN  A1
  #define C_PIN  A2
  #define D_PIN  A3
  #define E_PIN  21 
  #define LAT_PIN 10
  #define OE_PIN  9
  #define CLK_PIN 8
  #define PIN_E 9 // E pin for 64px high panels
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
#elif defined (FREENOVE_ESP32_S3_WROOM)
// 'Safe' pins, not overlapping any peripherals:
  #define R1_PIN 2
  #define G1_PIN 3
  #define B1_PIN 4
  #define R2_PIN 5
  #define G2_PIN 6
  #define B2_PIN 7
  #define A_PIN  A0
  #define B_PIN  A1
  #define C_PIN  A2
  #define D_PIN  A3
  #define E_PIN  21 
  #define LAT_PIN 10
  #define OE_PIN  9
  #define CLK_PIN 8
  #define PIN_E 9 // E pin for 64px high panels
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


/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
// Configure for your panel(s) as appropriate!
/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
#define PANEL_WIDTH 64
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

// gradient buffer
CRGB *ledbuff;

// LED array
CRGB leds[1];

unsigned long t1, t2, s1=0, s2=0, s3=0;
uint32_t ccount1, ccount2;

uint8_t color1 = 0, color2 = 0, color3 = 0;
uint16_t x,y;

const char *message = "* ESP32 I2S DMA *";