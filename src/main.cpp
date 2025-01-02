
#ifdef IDF_BUILD
#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/gpio.h>
#include "sdkconfig.h"
#endif

#include <Arduino.h>
#include "xtensa/core-macros.h"
#include "bitmaps.h"
#include "fonts/lequahyper20pt7b.h"       // Stylized font
#include <fonts/FreeSansBold18pt7b.h>     // Larger font
//#include <Wire.h>                       // For I2C sensors
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

uint16_t time_counter = 0, cycles = 0, fps = 0;
unsigned long fps_timer;

// gradient buffer
CRGB *ledbuff;
//

unsigned long t1, t2, s1=0, s2=0, s3=0;
uint32_t ccount1, ccount2;

uint8_t color1 = 0, color2 = 0, color3 = 0;
uint16_t x,y;

const char *message = "* ESP32 I2S DMA *";

// LumiFur Global Variables ---------------------------------------------------

// View switching
int currentView = 4; // Current & initial view being displayed
const int totalViews = 10; // Total number of views to cycle through

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

// Bitmap Drawing Functions ------------------------------------------------
void drawXbm565(int x, int y, int width, int height, const char *xbm, uint16_t color = 0xffff) {
    // Ensure width is padded to the nearest byte boundary
    int byteWidth = (width + 7) / 8;

    for (int j = 0; j < height; j++) {
        for (int i = 0; i < width; i++) {
            // Calculate byte and bit positions
            int byteIndex = j * byteWidth + (i / 8);
            int bitIndex = 7 - (i % 8); // Bits are stored MSB first

            // Check if the bit is set
            if (pgm_read_byte(&xbm[byteIndex]) & (1 << bitIndex)) {
                // Draw the pixel only if within display boundaries
                if (x + i >= 0 && y + j >= 0 && x + i < dma_display->width() && y + j < dma_display->height()) {
                    dma_display->drawPixel(x + i, y + j, color);
                }
            }
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

/*
  // Display percentage text
  char progressText[16];
  sprintf(progressText, "%d%%", loadingProgress);
  dma_display->setTextColor(dma_display->color565(255, 255, 255));
  dma_display->setCursor(barX - 8, barY + 14); // Position above the bar
  dma_display->print(progressText);
*/
  //delay(10);     // Short delay for smoother animation
}

// Helper function for ease-in-out quadratic easing
float easeInOutQuad(float t) {
return t < 0.5 ? 2 * t * t : -1 + (4 - 2 * t) * t;

dma_display->flipDMABuffer();
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
  //dma_display->flipDMABuffer();
  //dma_display->clearScreen();
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

    // Clear the screen before drawing
    dma_display->clearScreen();

    // Draw the rotating bitmaps
    drawRotatedBitmap(26, 10, spiral, 25, 25, currentAngle, color);
    drawRotatedBitmap(97, 10, spiralL, 25, 25, -currentAngle, color);

    // Flip the DMA buffer to update the display
    dma_display->flipDMABuffer();
    }
}


// Draw the blinking eyes
void blinkingEyes() {

  dma_display->clearScreen(); // Clear the display

  // Draw  eyes
  if (currentView == 4 || currentView == 5) {
  drawXbm565(0, 0, 32, 16, Eye, dma_display->color565(255, 255, 255));
  drawXbm565(96, 0, 32, 16, EyeL, dma_display->color565(255, 255, 255));
  }
  else if (currentView == 6) {
    drawXbm565(0, 0, 32, 12, semicircleeyes, dma_display->color565(255, 255, 255));
    drawXbm565(96, 0, 32, 12, semicircleeyes, dma_display->color565(255, 255, 255));
  }
  else if (currentView == 7) {
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
  else if (currentView == 8) {
    drawXbm565(0, 0, 24, 16, slanteyes, dma_display->color565(255, 255, 255));
    drawXbm565(104, 0, 24, 16, slanteyes, dma_display->color565(255, 255, 255));
  }
  else if (currentView == 9) {
  //  drawXbm565(15, 0, 25, 25, spiral, dma_display->color565(255, 255, 255));
  //  drawXbm565(88, 0, 25, 25, spiralL, dma_display->color565(255, 255, 255));
  }

  if (isBlinking) {
    // Calculate the height of the black box
    int boxHeight = map(blinkProgress, 0, 100, 0, 16); // Adjust height of blink. DEFAULT: From 0 to 16 pixels

    // Draw black boxes over the eyes
    dma_display->fillRect(0, 0, 32, boxHeight, 0);     // Cover the right eye
    dma_display->fillRect(96, 0, 32, boxHeight, 0);    // Cover the left eye
  }
  
  // Flip DMA buffer to update the display
  dma_display->flipDMABuffer(); // Update the display
}

void drawBlush() {
  //dma_display->clearScreen();
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

  //dma_display->flipDMABuffer(); // Update the display
  }

  // Set blush color based on brightness
  uint16_t blushColor = dma_display->color565(blushBrightness, 0, blushBrightness);

  // Clear only the blush area to prevent artifacts
  dma_display->fillRect(25, 12, 18, 13, 0); // Clear right blush area
  dma_display->fillRect(82, 12, 18, 13, 0); // Clear left blush area

     drawXbm565(35, 12, 11, 13, blush, blushColor);
     drawXbm565(30, 12, 11, 13, blush, blushColor);
     drawXbm565(25, 12, 11, 13, blush, blushColor);
     drawXbm565(82, 12, 11, 13, blushL, blushColor);
     drawXbm565(87, 12, 11, 13, blushL, blushColor);
     drawXbm565(92, 12, 11, 13, blushL, blushColor);

// Flip the DMA buffer after completing all rendering
//dma_display->flipDMABuffer(); // Update the display
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
  
  dma_display->flipDMABuffer();
}

void protoFaceTest() {
  dma_display->clearScreen(); // Clear the display
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
    dma_display->flipDMABuffer(); // Update the display
   // Draw the blush (with fading effect in view 2)
   if (currentView == 5) {
   drawBlush();
   //dma_display->flipDMABuffer(); // Update the display
}
   
   // Draw blinking eyes
   blinkingEyes();

   //dma_display->flipDMABuffer(); // Update the display
   //dma_display->show(); // Update the matrix display
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

  // Refresh display
  //dma_display->flipDMABuffer();
}

void setup() {

  Serial.begin(BAUD_RATE);
  Serial.println(F("*****************************************************"));
  Serial.println(F("*                      LumiFur                      *"));
  Serial.println(F("*****************************************************"));

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

dma_display->clearScreen();

/*
  ledbuff = (CRGB *)malloc(NUM_LEDS * sizeof(CRGB));  // allocate buffer for some tests
  if (ledbuff == nullptr) {
    Serial.println("Memory allocation for ledbuff failed!");
    while (1); // Stop execution
  }
*/

// Initialize accelerometer if MATRIXPORTAL_ESP32S3 is used
#if defined (ARDUINO_ADAFRUIT_MATRIXPORTAL_ESP32S3)
  //Adafruit_LIS3DH accel = Adafruit_LIS3DH();
#endif

randomSeed(analogRead(0)); // Seed the random number generator for randomized eye blinking

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
}

uint8_t wheelval = 0;

void displayCurrentView(int view) {
  static int previousView = -1; // Track the last active view
  //dma_display->clearScreen(); // Clear display buffer at start of each frame

if (view != previousView) {
  //dma_display->clearScreen(); // Clear the screen for a new view
    if (view == 5) {
      // Reset fade logic when entering the blush view
      blushFadeStartTime = millis();
      isBlushFadingIn = true;
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
  dma_display->clearScreen();
    displayLoadingBar();
    if (loadingProgress <= loadingMax)
    {
      displayLoadingBar(); // Update the loading bar
      loadingProgress++;   // Increment progress
      delay(50);           // Adjust speed of loading animation
    } else {
      // Reset or transition to another view
      loadingProgress = 0;
    }
    dma_display->flipDMABuffer();
    break;

  case 2: // Pattern plasma
    patternPlasma();
    //delay(10);
    //dma_display->flipDMABuffer();
    break;

  case 3:
    dma_display->clearScreen(); // Clear the display
    drawTransFlag();
    delay(20);
    dma_display->flipDMABuffer();
    break;

  case 4: // Normal
    protoFaceTest();
    updateBlinkAnimation(); // Update blink animation progress
    delay(20);              // Short delay for smoother animation
    break;

  case 5: // Blush with fade in effect
    protoFaceTest();
    updateBlinkAnimation(); // Update blink animation progress
    break;

  case 6: // Dialated pupils
    protoFaceTest();
    updateBlinkAnimation(); // Update blink animation progress
    break;

  case 7: // X eyes
    protoFaceTest();
    updateBlinkAnimation(); // Update blink animation progress
    break;

  case 8: // Slant eyes
    protoFaceTest();
    updateBlinkAnimation(); // Update blink animation progress
    break;

  case 9: // Spiral eyes
    protoFaceTest();
//  updateBlinkAnimation(); // Update blink animation progress
    updateRotatingSpiral();
    break;
/*
  case 10: // Slant eyes
    protoFaceTest();
    updateBlinkAnimation(); // Update blink animation progress
    break;
  
  case 11: // Spiral eyes
    protoFaceTest();
    updateBlinkAnimation(); // Update blink animation progress
    break;
*/
  default:
      // Optional: Handle unsupported views
      //dma_display->clearScreen();
      break;
  }
  //dma_display->clearScreen();
  dma_display->flipDMABuffer();
}


void loop(void) {
  // Non-blocking display update for the current view
  static unsigned long lastFrameTime = 0;
  unsigned long currentTime = millis();

  // Target frame duration (e.g., 30 FPS = ~33ms per frame)
  const unsigned long frameDuration = 11;

// Check if it's time for the next frame
  if (currentTime - lastFrameTime >= frameDuration) {
    lastFrameTime = currentTime;

    if (debounceButton(BUTTON_UP)) {
      currentView = (currentView + 1) % totalViews;
    }
    if (debounceButton(BUTTON_DOWN)) {
      currentView = (currentView - 1 + totalViews) % totalViews;
    }
    displayCurrentView(currentView);

    //dma_display->flipDMABuffer();
  }
}
