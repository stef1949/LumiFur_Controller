# 1 "/var/folders/gz/tmbwzlvj7wn4_24ztt7j3lp40000gn/T/tmpweu7gkhz"
#include <Arduino.h>
# 1 "/Users/stephanritchie/Documents/PlatformIO/Projects/LumiFur_02/src/LumiFur_02.ino"
# 9 "/Users/stephanritchie/Documents/PlatformIO/Projects/LumiFur_02/src/LumiFur_02.ino"
#include <Adafruit_Protomatter.h>
#include "fonts/lequahyper20pt7b.h"
#include <fonts/FreeSansBold18pt7b.h>
#include <FastLED.h>






#if defined(_VARIANT_MATRIXPORTAL_M4_)
  uint8_t rgbPins[] = {7, 8, 9, 10, 11, 12};
  uint8_t addrPins[] = {17, 18, 19, 20, 21};
  uint8_t clockPin = 14;
  uint8_t latchPin = 15;
  uint8_t oePin = 16;
  #define BUTTON_UP 2
  #define BUTTON_DOWN 3
#elif defined(ARDUINO_ADAFRUIT_MATRIXPORTAL_ESP32S3)
  uint8_t rgbPins[] = {42, 41, 40, 38, 39, 37};
  uint8_t addrPins[] = {45, 36, 48, 35, 21};
  uint8_t clockPin = 2;
  uint8_t latchPin = 47;
  uint8_t oePin = 14;
  #define BUTTON_UP 6
  #define BUTTON_DOWN 7

#elif defined(_VARIANT_FEATHER_M4_)
  uint8_t rgbPins[] = {6, 5, 9, 11, 10, 12};
  uint8_t addrPins[] = {A5, A4, A3, A2};
  uint8_t clockPin = 13;
  uint8_t latchPin = 0;
  uint8_t oePin = 1;
#elif defined(__SAMD51__)
  uint8_t rgbPins[] = {6, 5, 9, 11, 10, 12};
  uint8_t addrPins[] = {A5, A4, A3, A2};
  uint8_t clockPin = 13;
  uint8_t latchPin = 0;
  uint8_t oePin = 1;
#elif defined(_SAMD21_)
  uint8_t rgbPins[] = {6, 7, 10, 11, 12, 13};
  uint8_t addrPins[] = {0, 1, 2, 3};
  uint8_t clockPin = SDA;
  uint8_t latchPin = 4;
  uint8_t oePin = 5;
#elif defined(NRF52_SERIES)
  uint8_t rgbPins[] = {6, A5, A1, A0, A4, 11};
  uint8_t addrPins[] = {10, 5, 13, 9};
  uint8_t clockPin = 12;
  uint8_t latchPin = PIN_SERIAL1_RX;
  uint8_t oePin = PIN_SERIAL1_TX;
#elif USB_VID == 0x239A && USB_PID == 0x8113

  uint8_t rgbPins[] = {6, 5, 9, 11, 10, 12};
  uint8_t addrPins[] = {A5, A4, A3, A2};
  uint8_t clockPin = 13;
  uint8_t latchPin = RX;
  uint8_t oePin = TX;
#elif USB_VID == 0x239A && USB_PID == 0x80EB

  uint8_t rgbPins[] = {6, 5, 9, 11, 10, 12};
  uint8_t addrPins[] = {A5, A4, A3, A2};
  uint8_t clockPin = 13;
  uint8_t latchPin = RX;
  uint8_t oePin = TX;
#elif defined(ESP32)







  uint8_t rgbPins[] = {4, 12, 13, 14, 15, 21};
  uint8_t addrPins[] = {16, 17, 25, 26};
  uint8_t clockPin = 27;
  uint8_t latchPin = 32;
  uint8_t oePin = 33;
#elif defined(ARDUINO_TEENSY40)
  uint8_t rgbPins[] = {15, 16, 17, 20, 21, 22};
  uint8_t addrPins[] = {2, 3, 4, 5};
  uint8_t clockPin = 23;
  uint8_t latchPin = 6;
  uint8_t oePin = 9;
#elif defined(ARDUINO_TEENSY41)
  uint8_t rgbPins[] = {26, 27, 38, 20, 21, 22};
  uint8_t addrPins[] = {2, 3, 4, 5};
  uint8_t clockPin = 23;
  uint8_t latchPin = 6;
  uint8_t oePin = 9;
#elif defined(ARDUINO_ADAFRUIT_FEATHER_RP2040)






  uint8_t rgbPins[] = {8, 7, 9, 11, 10, 12};
  uint8_t addrPins[] = {25, 24, 29, 28};
  uint8_t clockPin = 13;
  uint8_t latchPin = 1;
  uint8_t oePin = 0;
#endif




const int matrixWidth = 128;
const int matrixHeight = 32;


int currentView = 1;
const int totalViews = 11;


int loadingProgress = 0;
int loadingMax = 100;


unsigned long lastEyeBlinkTime = 0;
unsigned long nextBlinkDelay = 1000;
int blinkProgress = 0;
bool isBlinking = false;

int blinkDuration = 300;
const int minBlinkDuration = 100;
const int maxBlinkDuration = 500;

const int minBlinkDelay = 1000;
const int maxBlinkDelay = 5000;


unsigned long blushFadeStartTime = 0;
const unsigned long blushFadeDuration = 2000;
bool isBlushFadingIn = true;
uint8_t blushBrightness = 0;


uint16_t time_counter = 0, cycles = 0, fps = 0;
unsigned long fps_timer;

CRGB currentColor;
CRGBPalette16 palettes[] = {LavaColors_p, HeatColors_p, RainbowColors_p, RainbowStripeColors_p, CloudColors_p};
CRGBPalette16 currentPalette = palettes[0];

CRGB ColorFromCurrentPalette(uint8_t index = 0, uint8_t brightness = 255, TBlendType blendType = LINEARBLEND) {
  return ColorFromPalette(currentPalette, index, brightness, blendType);
}
# 165 "/Users/stephanritchie/Documents/PlatformIO/Projects/LumiFur_02/src/LumiFur_02.ino"
Adafruit_Protomatter matrix(
  128,
  6,
  1, rgbPins,
  4, addrPins,
  clockPin, latchPin, oePin,
  true);
bool debounceButton(int pin);
void err(int x);
void displayLoadingBar();
void updateBlinkAnimation();
float easeInOutQuad(float t);
void blinkingEyes();
void drawBlush();
void drawTransFlag();
void protoFaceTest();
void patternPlasma();
void setup(void);
void displayCurrentView(int view);
void loop(void);
#line 176 "/Users/stephanritchie/Documents/PlatformIO/Projects/LumiFur_02/src/LumiFur_02.ino"
bool debounceButton(int pin) {
  static uint32_t lastPressTime = 0;
  uint32_t currentTime = millis();

  if (digitalRead(pin) == LOW && (currentTime - lastPressTime) > 200) {
    lastPressTime = currentTime;
    return true;
  }
  return false;
}




const PROGMEM uint8_t appleLogoApple_logo_black[] = {
 0x00, 0x18, 0x00, 0x00, 0x30, 0x00, 0x00, 0x70, 0x00, 0x00, 0xe0, 0x00, 0x00, 0xc0, 0x00, 0x1e,
 0x1e, 0x00, 0x7f, 0xff, 0x80, 0x7f, 0xff, 0x80, 0x7f, 0xfe, 0x00, 0xff, 0xfe, 0x00, 0xff, 0xfe,
 0x00, 0xff, 0xfe, 0x00, 0xff, 0xfe, 0x00, 0xff, 0xfe, 0x00, 0xff, 0xff, 0x00, 0x7f, 0xff, 0x80,
 0x7f, 0xff, 0x80, 0x3f, 0xff, 0x80, 0x3f, 0xff, 0x00, 0x1f, 0xfe, 0x00, 0x0f, 0x3c, 0x00
};


const int appleLogoallArray_LEN = 1;
const unsigned char* appleLogoallArray[1] = {
 appleLogoApple_logo_black
};




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
const PROGMEM uint8_t slant_eyes[] = {
 0xe0, 0x00, 0x00, 0x00, 0x00, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x7f, 0xc0, 0x00, 0x00, 0x00, 0x7f,
 0xfc, 0x00, 0x00, 0x00, 0x3f, 0xff, 0xc0, 0x00, 0x00, 0x3f, 0xff, 0xfc, 0x00, 0x00, 0x1f, 0xff,
 0xff, 0x80, 0x00, 0x07, 0xff, 0xff, 0xf8, 0x00, 0x03, 0xff, 0xff, 0xff, 0x80, 0x00, 0xff, 0xff,
 0xff, 0xc0, 0x00, 0x3f, 0xff, 0xff, 0xc0, 0x00, 0x0f, 0xff, 0xff, 0x80, 0x00, 0x00, 0xff, 0xfe,
 0x00, 0x00, 0x00, 0x07, 0xe0, 0x00
};

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

const PROGMEM uint8_t slant_eyesL[] = {
 0x00, 0x00, 0x00, 0x00, 0xe0, 0x00, 0x00, 0x00, 0x0f, 0xe0, 0x00, 0x00, 0x00, 0x7f, 0xc0, 0x00,
 0x00, 0x07, 0xff, 0xc0, 0x00, 0x00, 0x7f, 0xff, 0x80, 0x00, 0x07, 0xff, 0xff, 0x80, 0x00, 0x3f,
 0xff, 0xff, 0x00, 0x03, 0xff, 0xff, 0xfc, 0x00, 0x3f, 0xff, 0xff, 0xf8, 0x00, 0x7f, 0xff, 0xff,
 0xe0, 0x00, 0x7f, 0xff, 0xff, 0x80, 0x00, 0x3f, 0xff, 0xfe, 0x00, 0x00, 0x0f, 0xff, 0xe0, 0x00,
 0x00, 0x00, 0xfc, 0x00, 0x00, 0x00
  };


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




int16_t textX;
int16_t textY;
int16_t textMin;
char str[64];
int16_t ball[3][4] = {
  { 3, 0, 1, 1 },
  { 17, 15, 1, -1 },
  { 27, 4, -1, 1 }
};
uint16_t ballcolor[3];




void err(int x) {
  uint8_t i;
  pinMode(LED_BUILTIN, OUTPUT);
  for(i=1;;i++) {
    digitalWrite(LED_BUILTIN, i & 1);
    delay(x);
  }
}

void displayLoadingBar() {

  matrix.drawBitmap(23, 2, appleLogoApple_logo_black, 18, 21, matrix.color565(255, 255, 255));
  matrix.drawBitmap(88, 2, appleLogoApple_logo_black, 18, 21, matrix.color565(255, 255, 255));


  int barWidth = matrix.width() - 80;
  int barHeight = 5;
  int barX = (matrix.width() / 4) - (barWidth / 2);
  int barY = (matrix.height() - barHeight) / 2;
  int barRadius = 4;
  matrix.fillRoundRect(barX, (barY * 2), barWidth, barHeight, barRadius, matrix.color565(9, 9, 9));
  matrix.fillRoundRect(barX + 64, (barY * 2), barWidth, barHeight, barRadius, matrix.color565(9, 9, 9));


  int progressWidth = (barWidth - 2) * loadingProgress / loadingMax;
  matrix.fillRoundRect(barX + 1, (barY * 2) + 1, progressWidth, barHeight - 2, barRadius - 2, matrix.color565(255, 255, 255));
  matrix.fillRoundRect((barX + 1) + 64, (barY * 2) + 1, progressWidth, barHeight - 2, barRadius - 2, matrix.color565(255, 255, 255));
# 505 "/Users/stephanritchie/Documents/PlatformIO/Projects/LumiFur_02/src/LumiFur_02.ino"
  matrix.show();
  delay(10);
}

void updateBlinkAnimation() {
  unsigned long currentTime = millis();

  if (isBlinking) {
    unsigned long elapsed = currentTime - lastEyeBlinkTime;

    if (elapsed >= blinkDuration) {

      isBlinking = false;
      lastEyeBlinkTime = currentTime;
      nextBlinkDelay = random(minBlinkDelay, maxBlinkDelay);
    } else {

      float normalizedProgress = float(elapsed) / blinkDuration;
      blinkProgress = 100 * easeInOutQuad(normalizedProgress);
    }
  } else {

    if (currentTime - lastEyeBlinkTime >= nextBlinkDelay) {
      isBlinking = true;
      blinkProgress = 0;
      lastEyeBlinkTime = currentTime;
      blinkDuration = random(minBlinkDuration, maxBlinkDuration);
    }
  }
}


float easeInOutQuad(float t) {
  if (t < 0.5) {
    return 2 * t * t;
  }
  return -1 + (4 - 2 * t) * t;
}


void blinkingEyes() {


  if (currentView == 1 || currentView == 2) {
  matrix.drawBitmap(0, 0, Eye, 32, 16, matrix.color565(255, 255, 255));
  matrix.drawBitmap(96, 0, EyeL, 32, 16, matrix.color565(255, 255, 255));
  }
  if (currentView == 3) {
    matrix.drawBitmap(0, 0, semicircleeyes, 32, 12, matrix.color565(255, 255, 255));
    matrix.drawBitmap(96, 0, semicircleeyes, 32, 12, matrix.color565(255, 255, 255));
  }
  if (currentView == 4) {
    matrix.drawBitmap(0, 0, x_eyes, 31, 15, matrix.color565(255, 255, 255));
    matrix.drawBitmap(96, 0, x_eyes, 31, 15, matrix.color565(255, 255, 255));
  }
  if (currentView == 5) {
    matrix.drawBitmap(0, 0, Angry, 16, 8, matrix.color565(255, 255, 255));
    matrix.drawBitmap(96, 0, AngryL, 16, 8, matrix.color565(255, 255, 255));
  }
  if (currentView == 6) {
    matrix.drawBitmap(0, 0, Spooked, 16, 8, matrix.color565(255, 255, 255));
    matrix.drawBitmap(96, 0, SpookedL, 16, 8, matrix.color565(255, 255, 255));
  }
  if (currentView == 7) {
    matrix.drawBitmap(0, 0, slant_eyes, 35, 14, matrix.color565(255, 255, 255));
    matrix.drawBitmap(96, 0, slant_eyesL, 35, 14, matrix.color565(255, 255, 255));
  }

  if (isBlinking) {

    int boxHeight = map(blinkProgress, 0, 100, 0, 16);


    matrix.fillRect(0, 0, 32, boxHeight, 0);
    matrix.fillRect(96, 0, 32, boxHeight, 0);
  }

  matrix.show();
}

void drawBlush() {

  if (isBlushFadingIn) {
    unsigned long currentTime = millis();
    unsigned long elapsedTime = currentTime - blushFadeStartTime;

  if (elapsedTime <= blushFadeDuration) {
    blushBrightness = map(elapsedTime, 0, blushFadeDuration, 0, 255);
  } else {
      blushBrightness = 255;
      isBlushFadingIn = false;
    }
  }


  uint16_t blushColor = matrix.color565(blushBrightness, 0, blushBrightness);

     matrix.drawBitmap(35, 12, blush, 11, 13, blushColor);
     matrix.drawBitmap(30, 12, blush, 11, 13, blushColor);
     matrix.drawBitmap(25, 12, blush, 11, 13, blushColor);
     matrix.drawBitmap(82, 12, blushL, 11, 13, blushColor);
     matrix.drawBitmap(87, 12, blushL, 11, 13, blushColor);
     matrix.drawBitmap(92, 12, blushL, 11, 13, blushColor);
}

void drawTransFlag() {
  int stripeHeight = matrixHeight / 5;


  uint16_t lightBlue = matrix.color565(0, 102/2, 204/2);
  uint16_t pink = matrix.color565(255, 20/2, 147/2);
  uint16_t white = matrix.color565(255/2, 255/2, 255/2);


  matrix.fillRect(0, 0, matrixWidth, stripeHeight, lightBlue);
  matrix.fillRect(0, stripeHeight, matrixWidth, stripeHeight, pink);
  matrix.fillRect(0, stripeHeight * 2, matrixWidth, stripeHeight, white);
  matrix.fillRect(0, stripeHeight * 3, matrixWidth, stripeHeight, pink);
  matrix.fillRect(0, stripeHeight * 4, matrixWidth, stripeHeight, lightBlue);
}

void protoFaceTest() {
   matrix.drawBitmap(56, 10, nose, 8, 8, matrix.color565(255, 255, 255));
   matrix.drawBitmap(64, 10, noseL, 8, 8, matrix.color565(255, 255, 255));
   matrix.drawBitmap(0, 16, maw, 64, 16, matrix.color565(255, 255, 255));
   matrix.drawBitmap(64, 16, mawL, 64, 16, matrix.color565(255, 255, 255));
# 644 "/Users/stephanritchie/Documents/PlatformIO/Projects/LumiFur_02/src/LumiFur_02.ino"
   if (currentView == 2) {
   drawBlush();
}


   blinkingEyes();

   matrix.show();
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

  matrix.show();
  ++time_counter;
  ++cycles;
  ++fps;

  if (cycles >= 1024) {
    time_counter = 0;
    cycles = 0;
    currentPalette = palettes[random(0, sizeof(palettes) / sizeof(palettes[0]))];
  }
}

void setup(void) {
  Serial.begin(9600);


  ProtomatterStatus status = matrix.begin();
  Serial.print("Protomatter begin() status: ");
  Serial.println((int)status);
  if(status != PROTOMATTER_OK) {

    for(;;);
  }
randomSeed(analogRead(0));





  sprintf(str, "LumiFur_02 %dx%d RGB LED Matrix",
    matrix.width(), matrix.height());
  matrix.setFont(&FreeSansBold18pt7b);
  matrix.setTextWrap(false);
  matrix.setTextColor(0xFFFF);
  int16_t x1, y1;
  uint16_t w, h;
  matrix.getTextBounds(str, 0, 0, &x1, &y1, &w, &h);
  textMin = -w;
  textX = matrix.width();
  textY = matrix.height() / 2 - (y1 + h / 2);






  ballcolor[0] = matrix.color565(0, 20, 0);
  ballcolor[1] = matrix.color565(0, 0, 20);
  ballcolor[2] = matrix.color565(20, 0, 0);


  currentPalette = RainbowColors_p;







  #ifdef BUTTON_UP
    pinMode(BUTTON_UP, INPUT_PULLUP);
  #endif
  #ifdef BUTTON_DOWN
    pinMode(BUTTON_DOWN, INPUT_PULLUP);
  #endif
}

void displayCurrentView(int view) {
  static int previousView = -1;
  matrix.fillScreen(0);

if (view != previousView) {
      if (view == 2) {

      blushFadeStartTime = millis();
      isBlushFadingIn = true;
    }
    previousView = view;
  }

  switch (view) {

    case 0:






  matrix.setCursor(textX, textY);
  matrix.print(str);



  if((--textX) < textMin) textX = matrix.width();


  for(byte i=0; i<3; i++) {

    matrix.fillCircle(ball[i][0], ball[i][1], 5, ballcolor[i]);

    ball[i][0] += ball[i][2];
    ball[i][1] += ball[i][3];

    if((ball[i][0] == 0) || (ball[i][0] == (matrix.width() - 1)))
      ball[i][2] *= -1;
    if((ball[i][1] == 0) || (ball[i][1] == (matrix.height() - 1)))
      ball[i][3] *= -1;
  }
      break;

      case 1:
      protoFaceTest();
      updateBlinkAnimation();
      delay(20);
      break;

      case 2:
      protoFaceTest();
      updateBlinkAnimation();
      break;

      case 3:
      protoFaceTest();
      updateBlinkAnimation();
      break;

      case 4:
      protoFaceTest();
      updateBlinkAnimation();
      break;

      case 5:
      protoFaceTest();
      updateBlinkAnimation();
      break;

      case 6:
      protoFaceTest();
      updateBlinkAnimation();
      break;

      case 7:
      protoFaceTest();
      updateBlinkAnimation();
      break;

      case 8:
      drawTransFlag();
      delay(20);
      break;

      case 9:
      displayLoadingBar();
        if (loadingProgress <= loadingMax) {
    displayLoadingBar();
    loadingProgress++;
    delay(50);
  } else {

    loadingProgress = 0;
    }
    break;
       case 10:
    patternPlasma();

    break;
    }

  matrix.show();
}



void loop(void) {

  static unsigned long lastFrameTime = 0;
  unsigned long currentTime = millis();


  const unsigned long frameDuration = 15;


  if (currentTime - lastFrameTime >= frameDuration) {
    lastFrameTime = currentTime;

    if (debounceButton(BUTTON_UP)) {
      currentView = (currentView + 1) % totalViews;
    }
    if (debounceButton(BUTTON_DOWN)) {
      currentView = (currentView - 1 + totalViews) % totalViews;
    }

    displayCurrentView(currentView);
  }
}