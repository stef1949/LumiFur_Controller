/* ----------------------------------------------------------------------
Double-buffering (smooth animation) Protomatter library example.
PLEASE SEE THE "simple" EXAMPLE FOR AN INTRODUCTORY SKETCH.
Comments here pare down many of the basics and focus on the new concepts.

This example is written for a 64x32 matrix but can be adapted to others.
------------------------------------------------------------------------- */

#include <Adafruit_Protomatter.h>
//#include <Adafruit_LIS3DH.h>      // For accelerometer
#include <Fonts/FreeSansBold18pt7b.h> // Large friendly font

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
#elif defined(ARDUINO_ADAFRUIT_MATRIXPORTAL_ESP32S3) // MatrixPortal ESP32-S3
  uint8_t rgbPins[]  = {42, 41, 40, 38, 39, 37};
  uint8_t addrPins[] = {45, 36, 48, 35, 21};
  uint8_t clockPin   = 2;
  uint8_t latchPin   = 47;
  uint8_t oePin      = 14;
  #define BUTTON_UP 6
  #define BUTTON_DOWN 7
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

//Adafruit_LIS3DH accel = Adafruit_LIS3DH();
/////////////////////////// Bitmaps

// 'Apple_logo_black', 64x32px
const PROGMEM uint8_t appleLogoApple_logo_black[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0e, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x3e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3c, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x7c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x78, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x7c, 0x0f, 0x80, 0x00, 0x00, 0x00, 0x00, 0x01, 0xff, 0xff, 0xe0, 0x00, 0x00, 
	0x00, 0x00, 0x03, 0xff, 0xff, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x03, 0xff, 0xff, 0xf0, 0x00, 0x00, 
	0x00, 0x00, 0x07, 0xff, 0xff, 0xe0, 0x00, 0x00, 0x00, 0x00, 0x07, 0xff, 0xff, 0xc0, 0x00, 0x00, 
	0x00, 0x00, 0x0f, 0xff, 0xff, 0x80, 0x00, 0x00, 0x00, 0x00, 0x0f, 0xff, 0xff, 0x80, 0x00, 0x00, 
	0x00, 0x00, 0x0f, 0xff, 0xff, 0x80, 0x00, 0x00, 0x00, 0x00, 0x0f, 0xff, 0xff, 0x80, 0x00, 0x00, 
	0x00, 0x00, 0x0f, 0xff, 0xff, 0x80, 0x00, 0x00, 0x00, 0x00, 0x0f, 0xff, 0xff, 0x80, 0x00, 0x00, 
	0x00, 0x00, 0x0f, 0xff, 0xff, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x0f, 0xff, 0xff, 0xe0, 0x00, 0x00, 
	0x00, 0x00, 0x07, 0xff, 0xff, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x07, 0xff, 0xff, 0xf8, 0x00, 0x00, 
	0x00, 0x00, 0x03, 0xff, 0xff, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x03, 0xff, 0xff, 0xf0, 0x00, 0x00, 
	0x00, 0x00, 0x01, 0xff, 0xff, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x01, 0xff, 0xff, 0xe0, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0xff, 0xff, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7f, 0xff, 0xc0, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x3e, 0x1f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
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
    B00100000, B00000000, B00000000, B00000000,
             B01111000, B00000000, B00000000, B00000000,
             B11011110, B00000000, B00000000, B00000000,
             B11000111, B10000000, B00000000, B11100000,
             B11111111, B11100000, B00000111, B11111000,
             B00000000, B01111000, B00011110, B00011110,
             B00000000, B00011110, B01111000, B00000111,
             B00000000, B00000111, B11100000, B00000001
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
const PROGMEM uint8_t Eye[] = {B00001111, B00000000,
             B00111111, B11100000,
             B01111111, B11111000,
             B11111111, B11111110,
             B11110000, B00000111,
             B01100000, B00000001,
             B00000000, B00000000,
             B00000000, B00000000
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
             B00001110, B00000000}
             ;

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
    B00000000, B00000000, B00000000, B00000100,
              B00000000, B00000000, B00000000, B00011110,
              B00000000, B00000000, B00000000, B01111011,
              B00000111, B00000000, B00000001, B11100011,
              B00011111, B11100000, B00000111, B11111111,
              B01111000, B01111000, B00011110, B00000000,
              B11100000, B00011110, B01111000, B00000000,
              B10000000, B00000111, B11100000, B00000000
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
    B00000000, B11110000,
              B00000111, B11111100,
              B00011111, B11111110,
              B01111111, B11111111,
              B11100000, B00001111,
              B10000000, B00000110,
              B00000000, B00000000,
              B00000000, B00000000
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

void setup(void) {
  Serial.begin(9600);

  // Initialize matrix...
  ProtomatterStatus status = matrix.begin();
  Serial.print("Protomatter begin() status: ");
  Serial.println((int)status);
  if(status != PROTOMATTER_OK) {
    // DO NOT CONTINUE if matrix setup encountered an error.
    for(;;);
  }

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

  #ifdef BUTTON_UP
    pinMode(BUTTON_UP, INPUT);
  #endif
  #ifdef BUTTON_DOWN
    pinMode(BUTTON_DOWN, INPUT);
  #endif
}

// LOOP - RUNS REPEATEDLY AFTER SETUP --------------------------------------

void loop(void) {
  // Every frame, we clear the background and draw everything anew.
  // This happens "in the background" with double buffering, that's
  // why you don't see everything flicker. It requires double the RAM,
  // so it's not practical for every situation.

  matrix.fillScreen(0); // Fill background black

  // Draw the big scrolly text
  matrix.setCursor(textX, textY);
  matrix.print(str);

  // Update text position for next frame. If text goes off the
  // left edge, reset its position to be off the right edge.
  if((--textX) < textMin) textX = matrix.width();

  // Draw the three bouncy balls on top of the text...
  for(byte i=0; i<3; i++) {
    // Draw 'ball'
    matrix.fillCircle(ball[i][0], ball[i][1], 5, ballcolor[i]);
    // Update ball's X,Y position for next frame
    ball[i][0] += ball[i][2];
    ball[i][1] += ball[i][3];
    // Bounce off edges
    if((ball[i][0] == 0) || (ball[i][0] == (matrix.width() - 1)))
      ball[i][2] *= -1;
    if((ball[i][1] == 0) || (ball[i][1] == (matrix.height() - 1)))
      ball[i][3] *= -1;
  }

#ifdef BUTTON_UP
  if (digitalRead(BUTTON_UP) == LOW) {
    // Show apple logo
    matrix.fillScreen(0); // Clear the screen
    matrix.drawBitmap(32, 0, appleLogoApple_logo_black, 64, 32, matrix.color565(255, 255, 255));
  }
#endif

#ifdef BUTTON_DOWN
  if (digitalRead(BUTTON_DOWN) == LOW) {
    // Show current view (scrolling text and bouncy balls)
    matrix.fillScreen(0); // Fill background black

    // Draw the big scrolly text
    matrix.setCursor(textX, textY);
    matrix.print(str);

    // Update text position for next frame. If text goes off the
    // left edge, reset its position to be off the right edge.
    if((--textX) < textMin) textX = matrix.width();

    // Draw the three bouncy balls on top of the text...
    for(byte i=0; i<3; i++) {
      // Draw 'ball'
      matrix.fillCircle(ball[i][0], ball[i][1], 5, ballcolor[i]);
      // Update ball's X,Y position for next frame
      ball[i][0] += ball[i][2];
      ball[i][1] += ball[i][3];
      // Bounce off edges
      if((ball[i][0] == 0) || (ball[i][0] == (matrix.width() - 1)))
        ball[i][2] *= -1;
      if((ball[i][1] == 0) || (ball[i][1] == (matrix.height() - 1)))
        ball[i][3] *= -1;
    }
  }
#endif

  // AFTER DRAWING, A show() CALL IS REQUIRED TO UPDATE THE MATRIX!

  matrix.show();

  delay(20); // 20 milliseconds = ~50 frames/second
}
