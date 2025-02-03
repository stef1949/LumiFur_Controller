
#ifdef IDF_BUILD
#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/gpio.h>
#include "sdkconfig.h"
#endif

#define DEBUG_BLE
#define DEBUG_VIEWS

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

//BLE Libraries
#include <NimBLEDevice.h>

// BLE UUIDs
#define SERVICE_UUID        "01931c44-3867-7740-9867-c822cb7df308"
#define CHARACTERISTIC_UUID "01931c44-3867-7427-96ab-8d7ac0ae09fe"
#define CONFIG_CHARACTERISTIC_UUID "01931c44-3867-7427-96ab-8d7ac0ae09ff"
#define TEMPERATURE_CHARACTERISTIC_UUID "01931c44-3867-7b5d-9774-18350e3e27db"  // Unique UUID for temperature data
#define ULTRASOUND_CHARACTERISTIC_UUID "01931c44-3867-7b5d-9732-12460e3a35db"

//#define DESC_USER_DESC_UUID  0x2901  // User Description descriptor
//#define DESC_FORMAT_UUID     0x2904  // Presentation Format descriptor

// HUB75E port pinout
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
  #define PIN_E 21 // E pin for 64px high panels
  // Button pins
  #define BUTTON_UP 6
  #define BUTTON_DOWN 7
  //#include <Adafruit_LIS3DH.h>      // Library for built-in For accelerometer
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

// gradient buffer
CRGB *ledbuff;

// LED array
CRGB leds[1];

unsigned long t1, t2, s1=0, s2=0, s3=0;
uint32_t ccount1, ccount2;

uint8_t color1 = 0, color2 = 0, color3 = 0;
uint16_t x,y;

const char *message = "* ESP32 I2S DMA *";


//#include "EffectsLayer.hpp" // FastLED CRGB Pixel Buffer for which the patterns are drawn
//EffectsLayer effects(VPANEL_W, VPANEL_H);



// LumiFur Global Variables ---------------------------------------------------

// View switching
uint8_t currentView = 4; // Current & initial view being displayed
const int totalViews = 11; // Total number of views to cycle through
//Maw switching
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

// Variables for plasma effect
uint16_t time_counter = 0, cycles = 0, fps = 0;
unsigned long fps_timer;

CRGB currentColor;
CRGBPalette16 palettes[] = {ForestColors_p, LavaColors_p, HeatColors_p, RainbowColors_p, CloudColors_p};
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


////////////////////////////////////////////
/////////////////BLE CONFIG/////////////////
////////////////////////////////////////////

bool deviceConnected = false;
bool oldDeviceConnected = false;

// BLE Server pointers
NimBLEServer* pServer = nullptr;
NimBLECharacteristic* pCharacteristic = nullptr;
NimBLECharacteristic* pFaceCharacteristic = nullptr;
NimBLECharacteristic* pTemperatureCharacteristic = nullptr;
NimBLECharacteristic* pConfigCharacteristic = nullptr;

// Class to handle BLE server callbacks
class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override {
        deviceConnected = true;
        Serial.printf("Client connected: %s\n", connInfo.getAddress().toString().c_str());

        /**
         *  We can use the connection handle here to ask for different connection parameters.
         *  Args: connection handle, min connection interval, max connection interval
         *  latency, supervision timeout.
         *  Units; Min/Max Intervals: 1.25 millisecond increments.
         *  Latency: number of intervals allowed to skip.
         *  Timeout: 10 millisecond increments.
         */
        pServer->updateConnParams(connInfo.getConnHandle(), 24, 48, 0, 180);
    }

    void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override {
        deviceConnected = false;
        Serial.println("Client disconnected - advertising");
        NimBLEDevice::startAdvertising();
    }

    void onMTUChange(uint16_t MTU, NimBLEConnInfo& connInfo) override {
        Serial.printf("MTU updated: %u for connection ID: %u\n", MTU, connInfo.getConnHandle());
    }

    /********************* Security handled here *********************/
    uint32_t onPassKeyDisplay() override {
        Serial.printf("Server Passkey Display\n");
        /**
         * This should return a random 6 digit number for security
         *  or make your own static passkey as done here.
         */
        return 123456;
    }

    void onConfirmPassKey(NimBLEConnInfo& connInfo, uint32_t pass_key) override {
        Serial.printf("The passkey YES/NO number: %" PRIu32 "\n", pass_key);
        /** Inject false if passkeys don't match. */
        NimBLEDevice::injectConfirmPasskey(connInfo, true);
    }

    void onAuthenticationComplete(NimBLEConnInfo& connInfo) override {
        /** Check that encryption was successful, if not we disconnect the client */
        if (!connInfo.isEncrypted()) {
            NimBLEDevice::getServer()->disconnect(connInfo.getConnHandle());
            Serial.printf("Encrypt connection failed - disconnecting client\n");
            return;
        }

        Serial.printf("Secured connection to: %s\n", connInfo.getAddress().toString().c_str());
    }

} serverCallbacks;

void displayCurrentView(int view);

// Class to handle characteristic callbacks
class CharacteristicCallbacks : public NimBLECharacteristicCallbacks {
  void onRead(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) override {
        uint8_t viewValue = static_cast<uint8_t>(currentView);
        pCharacteristic->setValue(&viewValue, 1);
        Serial.printf("Read request - returned view: %d\n", viewValue);
    }

  void onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) {
        std::string value = pCharacteristic->getValue();
        if(value.length() > 0) {
            uint8_t newView = value[0];
            if (newView >= 1 && newView <=12 && newView != currentView) {
                currentView = newView;
                //viewChanged = true;
                Serial.printf("Write request - new view: %d\n", currentView);
                pCharacteristic->notify();
            }
        }
    }

    void onStatus(NimBLECharacteristic* pCharacteristic, int code) override {
        Serial.printf("Notification/Indication return code: %d, %s\n", code, NimBLEUtils::returnCodeToString(code));
    }
    void onSubscribe(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo, uint16_t subValue) override {
        std::string str  = "Client ID: ";
        str             += connInfo.getConnHandle();
        str             += " Address: ";
        str             += connInfo.getAddress().toString();
        if (subValue == 0) {
            str += " Unsubscribed to ";
        } else if (subValue == 1) {
            str += " Subscribed to notifications for ";
        } else if (subValue == 2) {
            str += " Subscribed to indications for ";
        } else if (subValue == 3) {
            str += " Subscribed to notifications and indications for ";
        }
        str += std::string(pCharacteristic->getUUID());

        Serial.printf("%s\n", str.c_str());
    }
} chrCallbacks; 

class DescriptorCallbacks : public NimBLEDescriptorCallbacks {
    void onWrite(NimBLEDescriptor* pDescriptor, NimBLEConnInfo& connInfo) override {
        std::string dscVal = pDescriptor->getValue();
        Serial.printf("Descriptor written value: %s\n", dscVal.c_str());
    }

    void onRead(NimBLEDescriptor* pDescriptor, NimBLEConnInfo& connInfo) override {
        Serial.printf("%s Descriptor read\n", pDescriptor->getUUID().toString().c_str());
    }
} dscCallbacks;

////////////////////////////////////////////
//////////////////BLE SETUP/////////////////
////////////////////////////////////////////


//non-blocking LED status functions (Neopixel)
void fadeInAndOutLED(uint8_t r, uint8_t g, uint8_t b) {
    static int brightness = 0;
    static int step = 5;
    static unsigned long lastUpdate = 0;
    const uint8_t delayTime = 5;

    if (millis() - lastUpdate < delayTime) return;
    lastUpdate = millis();

    brightness += step;
    if (brightness >= 255 || brightness <= 0) step *= -1;
    
    statusPixel.setPixelColor(0, 
        (r * brightness) / 255,
        (g * brightness) / 255,
        (b * brightness) / 255);
    statusPixel.show();
}

void handleBLEConnection() {
    if (deviceConnected != oldDeviceConnected) {
        if (deviceConnected) {
            statusPixel.setPixelColor(0, 0, 255, 0); // Green when connected
        } else {
            statusPixel.setPixelColor(0, 0, 0, 0); // Off when disconnected
            NimBLEDevice::startAdvertising();
        }
        statusPixel.show();
        oldDeviceConnected = deviceConnected;
    }
    
    if (!deviceConnected) {
        fadeInAndOutLED(0, 0, 255); // Blue fade when disconnected
    }
}

void fadeBlueLED() {
    fadeInAndOutLED(0, 0, 255); // Blue color
}

//Temp Non-Blocking Variables
unsigned long temperatureMillis = 0;
const unsigned long temperatureInterval = 1000; // 1 second interval for temperature update

void updateTemperature() {
  unsigned long currentMillis = millis();

  // Check if enough time has passed to update temperature
  if (currentMillis - temperatureMillis >= temperatureInterval) {
    temperatureMillis = currentMillis;

    if (deviceConnected) {
      // Read the ESP32's internal temperature
      float temperature = temperatureRead();  // Temperature in Celsius

      // Convert temperature to string and send over BLE
      char tempStr[12];
      snprintf(tempStr, sizeof(tempStr), "%.1f°C", temperature);
      pTemperatureCharacteristic->setValue(tempStr);
      pTemperatureCharacteristic->notify();

      // Optional: Debug output to serial
      Serial.print("Internal Temperature: ");
      Serial.println(tempStr);
    }
  }
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

void drawPlasmaXbm(int x, int y, int width, int height, const char *xbm, 
                 uint8_t time_offset = 0, float scale = 1.0) {
  int byteWidth = (width + 7) / 8;
  uint16_t plasmaSpeed = 10; // Lower = slower animation
  
  for (int j = 0; j < height; j++) {
    for (int i = 0; i < width; i++) {
      if (pgm_read_byte(&xbm[j * byteWidth + (i / 8)]) & (1 << (7 - (i % 8)))) {
        // Plasma calculation specific to pixel position
        uint8_t v = 
          sin8((x + i) * scale + time_counter) + 
          cos8((y + j) * scale + time_counter/2) + 
          sin8((x + i + y + j) * scale * 0.5 + time_counter/3);
        
        CRGB color = ColorFromPalette(currentPalette, v + time_offset);
        uint16_t rgb565 = dma_display->color565(color.r, color.g, color.b);
        
        dma_display->drawPixel(x + i, y + j, rgb565);
      }
    }
  }
}

void drawPlasmaFace() {
  // Draw eyes with different plasma parameters
  drawPlasmaXbm(0, 0, 32, 16, Eye, 0, 1.0);     // Right eye
  drawPlasmaXbm(96, 0, 32, 16, EyeL, 128, 1.0); // Left eye (phase offset)
  
  // Nose with finer pattern
  drawPlasmaXbm(56, 10, 8, 8, nose, 64, 2.0);
  drawPlasmaXbm(64, 10, 8, 8, noseL, 64, 2.0);
  
  // Mouth with larger pattern
  //drawPlasmaXbm(0, 16, 64, 16, maw, 32, 2.0);
  //drawPlasmaXbm(64, 16, 64, 16, mawL, 32, 2.0);
  drawPlasmaXbm(0, 10, 64, 22, maw2Closed, 32, 0.5);
  drawPlasmaXbm(64, 10, 64, 22, maw2ClosedL, 32, 0.5);
}

void updatePlasmaFace() {
  static unsigned long lastUpdate = 0;
  const uint16_t frameDelay = 50; // Slower update for smoother transition
  
  if (millis() - lastUpdate > frameDelay) {
    lastUpdate = millis();
    time_counter += 5; // Speed of plasma animation
    
    // Cycle palette every 10 seconds
    if (millis() % 10000 < frameDelay) {
      currentPalette = palettes[random(0, sizeof(palettes)/sizeof(palettes[0]))];
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

// Easing functions with bounds checking
float easeInQuad(float t) {
  return (t >= 1.0f) ? 1.0f : t * t;
}

float easeOutQuad(float t) {
  return (t <= 0.0f) ? 0.0f : 1.0f - (1.0f - t) * (1.0f - t);
}

// Blink animation control with initialization
struct {
  unsigned long startTime = 0;
  unsigned long durationMs = 200;  // Default duration
  unsigned long closeDuration = 50;
  unsigned long holdDuration = 20;
  unsigned long openDuration = 50;
} blinkState;

void initBlinkState() {
  blinkState.startTime = 0;
  blinkState.durationMs = 200;
  blinkState.closeDuration = 50;
  blinkState.holdDuration = 20;
  blinkState.openDuration = 50;
}

void updateBlinkAnimation() {
  static bool initialized = false;
  if (!initialized) {
    initBlinkState();
    initialized = true;
  }

  const unsigned long currentTime = millis();
  
  if (isBlinking) {
    const unsigned long elapsed = currentTime - blinkState.startTime;
    
    // Safety check for time overflow
    if (elapsed > blinkState.durationMs * 2) {
      isBlinking = false;
      return;
    }

    // Phase calculations with duration safeguards
    const unsigned long totalDuration = blinkState.closeDuration + 
                                      blinkState.holdDuration + 
                                      blinkState.openDuration;
    if (totalDuration == 0) {
      isBlinking = false;
      return;
    }

    if (elapsed < blinkState.closeDuration) {
      // Closing phase
      const float progress = (float)elapsed / blinkState.closeDuration;
      blinkProgress = 100 * easeInQuad(progress);
    } 
    else if (elapsed < blinkState.closeDuration + blinkState.holdDuration) {
      // Hold phase
      blinkProgress = 100;
    } 
    else if (elapsed < totalDuration) {
      // Opening phase
      const unsigned long openElapsed = elapsed - 
                                      (blinkState.closeDuration + 
                                       blinkState.holdDuration);
      const float progress = (float)openElapsed / blinkState.openDuration;
      blinkProgress = 100 * (1.0f - easeOutQuad(progress));
    } 
    else {
      // End blink
      isBlinking = false;
      blinkProgress = 0;
      lastEyeBlinkTime = currentTime;
      nextBlinkDelay = random(minBlinkDelay, maxBlinkDelay);
    }
  } 
  else {
    if (currentTime - lastEyeBlinkTime >= nextBlinkDelay) {
      // Initialize new blink with validated durations
      isBlinking = true;
      blinkState.startTime = currentTime;
      
      // Constrain durations to safe ranges
      blinkState.durationMs = constrain(random(minBlinkDuration, maxBlinkDuration), 
                                      50, 1000);
      
      // Calculate phases as percentages of total duration
      blinkState.closeDuration = blinkState.durationMs * random(25, 35) / 100;
      blinkState.holdDuration = blinkState.durationMs * random(5, 15) / 100;
      blinkState.openDuration = blinkState.durationMs - 
                              blinkState.closeDuration - 
                              blinkState.holdDuration;

      // Ensure minimum durations of 1 frame (16ms @60Hz)
      blinkState.closeDuration = max(blinkState.closeDuration, 16UL);
      blinkState.holdDuration = max(blinkState.holdDuration, 16UL);
      blinkState.openDuration = max(blinkState.openDuration, 16UL);
    }
  }
}

// Optimized rotation function
void drawRotatedBitmap(int16_t x, int16_t y, const uint8_t *bitmap, 
                      uint16_t w, uint16_t h, float cosA, float sinA, 
                      uint16_t color) {
  const int16_t cx = w/2, cy = h/2; // Center point
  
  for (int16_t j = 0; j < h; j++) {
    for (int16_t i = 0; i < w; i++) {
      if (pgm_read_byte(&bitmap[j * ((w+7)/8) + (i/8)]) & (1 << (7 - (i%8)))) {
        // Calculate rotated position
        int16_t dx = i - cx;
        int16_t dy = j - cy;
        int16_t newX = x + lround(dx * cosA - dy * sinA);
        int16_t newY = y + lround(dx * sinA + dy * cosA);
        
        // Boundary check and draw
        if (newX >= 0 && newX < PANE_WIDTH && newY >= 0 && newY < PANE_HEIGHT) {
          dma_display->drawPixel(newX, newY, color);
        }
      }
    }
  }
}

void updateRotatingSpiral() {
  static unsigned long lastUpdate = 0;
  static float currentAngle = 0;
  const unsigned long frameInterval = 11; // ~90 FPS (11ms per frame)
  unsigned long currentTime = millis();

  if (currentTime - lastUpdate < frameInterval) return;
  lastUpdate = currentTime;

  // Calculate color once per frame
  static uint8_t colorIndex = 0;
  colorIndex += 2; // Slower color transition
  CRGB currentColor = ColorFromPalette(currentPalette, colorIndex);
  uint16_t color = dma_display->color565(currentColor.r, currentColor.g, currentColor.b);
   
  // Clear the screen before drawing
  //dma_display->clearScreen();

  // Optimized rotation with pre-calculated values
  float radians = currentAngle * PI / 180.0;
  float cosA = cos(radians);
  float sinA = sin(radians);
  currentAngle = fmod(currentAngle + 6, 360); // Slower rotation (6° per frame)
  
  //Clear previous spiral frame
  //dma_display->fillRect(24, 15, 32, 25, 0);
  //dma_display->fillRect(105, 15, 32, 25, 0);
  // Draw both spirals using pre-transformed coordinates
  drawRotatedBitmap(24, 15, spiral, 25, 25,  cosA, sinA, color);
  drawRotatedBitmap(105, 15, spiralL, 25, 25,  cosA, sinA, color);
}



// Draw the blinking eyes
void blinkingEyes() {

  //dma_display->clearScreen(); // Clear the display

  // Draw  eyes
  if (currentView == 4 || currentView == 5) {
    //drawXbm565(0, 0, 32, 16, Eye, dma_display->color565(255, 255, 255));
    //drawXbm565(96, 0, 32, 16, EyeL, dma_display->color565(255, 255, 255));
    drawPlasmaXbm(0, 0, 32, 16, Eye, 0, 1.0);     // Right eye
    drawPlasmaXbm(96, 0, 32, 16, EyeL, 128, 1.0); // Left eye (phase offset)
  }
  else if (currentView == 6) {
    //drawXbm565(0, 0, 32, 12, semicircleeyes, dma_display->color565(255, 255, 255));
    //drawXbm565(96, 0, 32, 12, semicircleeyes, dma_display->color565(255, 255, 255));
    drawPlasmaXbm(0, 0, 32, 12, semicircleeyes, 0, 1.0);     // Right eye
    drawPlasmaXbm(96, 0, 32, 12, semicircleeyes, 128, 1.0); // Left eye (phase offset)
  }
  else if (currentView == 7) {
    //drawXbm565(0, 0, 31, 15, x_eyes, dma_display->color565(255, 255, 255));
    //drawXbm565(96, 0, 31, 15, x_eyes, dma_display->color565(255, 255, 255));
    drawPlasmaXbm(0, 0, 31, 15, x_eyes, 0, 1.0);     // Right eye
    drawPlasmaXbm(96, 0, 31, 15, x_eyes, 128, 1.0); // Left eye (phase offset)
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
    //drawXbm565(0, 0, 24, 16, slanteyes, dma_display->color565(255, 255, 255));
    //drawXbm565(104, 0, 24, 16, slanteyesL, dma_display->color565(255, 255, 255));
    drawPlasmaXbm(0, 0, 24, 16, slanteyes, 0, 1.0);     // Right eye
    drawPlasmaXbm(104, 0, 24, 16, slanteyesL, 128, 1.0); // Left eye (phase offset)
  }
  else if (currentView == 9) {
  //  drawXbm565(15, 0, 25, 25, spiral, dma_display->color565(255, 255, 255));
  //  drawXbm565(88, 0, 25, 25, spiralL, dma_display->color565(255, 255, 255));
  }

  if (isBlinking) {
    // Calculate the height of the black box
    int boxHeight = map(blinkProgress, 0, 100, 0, 20); // Adjust height of blink. DEFAULT: From 0 to 16 pixels

    // Draw black boxes over the eyes
    dma_display->fillRect(0, 0, 32, boxHeight, 0);     // Cover the right eye
    dma_display->fillRect(96, 0, 32, boxHeight, 0);    // Cover the left eye
  }
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
  }

  // Set blush color based on brightness
  uint16_t blushColor = dma_display->color565(blushBrightness, 0, blushBrightness);

  // Clear only the blush area to prevent artifacts
  dma_display->fillRect(45, 1, 18, 13, 0); // Clear right blush area
  dma_display->fillRect(72, 1, 18, 13, 0); // Clear left blush area

     drawXbm565(45, 1, 11, 13, blush, blushColor);
     drawXbm565(40, 1, 11, 13, blush, blushColor);
     drawXbm565(35, 1, 11, 13, blush, blushColor);
     drawXbm565(72, 1, 11, 13, blushL, blushColor);
     drawXbm565(77, 1, 11, 13, blushL, blushColor);
     drawXbm565(82, 1, 11, 13, blushL, blushColor);
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
}

void protoFaceTest() {
  dma_display->clearScreen(); // Clear the display

   // Draw Nose
   //drawXbm565(56, 2, 8, 8, nose, dma_display->color565(255, 255, 255));
   //drawXbm565(64, 2, 8, 8, noseL, dma_display->color565(255, 255, 255));
   //drawPlasmaXbm(56, 10, 8, 8, nose, 64, 2.0);
   //drawPlasmaXbm(64, 10, 8, 8, noseL, 64, 2.0);
   // Draw blinking eyes
   blinkingEyes();

   // Draw mouth
   //drawXbm565(0, 16, 64, 16, maw, dma_display->color565(255, 255, 255));
   //(64, 16, 64, 16, mawL, dma_display->color565(255, 255, 255));
   //drawXbm565(0, 0, Eye, 32, 16, dma_display->color565(255, 255, 255));
   //drawXbm565(96, 0, EyeL, 32, 16, dma_display->color565(255, 255, 255));

   //drawXbm565(0, 10, 64, 22, maw2Closed, dma_display->color565(255, 255, 255));
   //drawXbm565(64, 10, 64, 22, maw2ClosedL, dma_display->color565(255, 255, 255));
   drawPlasmaXbm(0, 10, 64, 22, maw2Closed, 0, 1.0);     // Right eye
   drawPlasmaXbm(64, 10, 64, 22, maw2ClosedL, 128, 1.0); // Left eye (phase offset)

   drawPlasmaXbm(56, 10, 8, 8, nose, 64, 2.0);
   drawPlasmaXbm(64, 10, 8, 8, noseL, 64, 2.0);

   if (currentView == 5) {
   drawBlush();
   }
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
}

void setup() {

  Serial.begin(BAUD_RATE);
  Serial.print(F("*****************************************************"));
  Serial.print(F("*                      LumiFur                      *"));
  Serial.print(F("*****************************************************"));
    
    Serial.print("Initializing BLE...");
    NimBLEDevice::init("LumiFur_Controller");
    NimBLEDevice::setPower(ESP_PWR_LVL_P9); // Power level 9 (highest) for best range
    NimBLEDevice::setSecurityAuth(/*BLE_SM_PAIR_AUTHREQ_BOND | BLE_SM_PAIR_AUTHREQ_MITM |*/ BLE_SM_PAIR_AUTHREQ_SC);
    pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(&serverCallbacks);

    NimBLEService* pService = pServer->createService(SERVICE_UUID);
    
     // Face control characteristic with encryption
    NimBLECharacteristic* pFaceCharacteristic =
      pService->createCharacteristic(
        CHARACTERISTIC_UUID,
        NIMBLE_PROPERTY::READ |
        NIMBLE_PROPERTY::WRITE |
        NIMBLE_PROPERTY::NOTIFY |
        NIMBLE_PROPERTY::READ_ENC | // only allow reading if paired / encrypted
        NIMBLE_PROPERTY::WRITE_ENC  // only allow writing if paired / encrypted
    );
    
    // Set initial view value
    uint8_t viewValue = static_cast<uint8_t>(currentView);
    pFaceCharacteristic->setValue(&viewValue, 1);
    pFaceCharacteristic->setCallbacks(&chrCallbacks);

    // Temperature characteristic with encryption
    pTemperatureCharacteristic = 
      pService->createCharacteristic(
        TEMPERATURE_CHARACTERISTIC_UUID,
        NIMBLE_PROPERTY::READ |
        NIMBLE_PROPERTY::NOTIFY |
        NIMBLE_PROPERTY::READ_ENC
    );
    
    // Config characteristic with encryption
    pConfigCharacteristic = pService->createCharacteristic(
        CONFIG_CHARACTERISTIC_UUID,
        NIMBLE_PROPERTY::READ |
        NIMBLE_PROPERTY::WRITE |
        NIMBLE_PROPERTY::NOTIFY |
        NIMBLE_PROPERTY::READ_ENC |
        NIMBLE_PROPERTY::WRITE_ENC
    );

    // Set up descriptors
    NimBLE2904* pFormat2904 = pFaceCharacteristic->create2904();
    pFormat2904->setFormat(NimBLE2904::FORMAT_UINT8);
    pFormat2904->setUnit(0x2700); // Unit-less number
    pFormat2904->setCallbacks(&dscCallbacks);

    // Add user description descriptor
    NimBLEDescriptor* pDesc = 
        pFaceCharacteristic->createDescriptor(
            "2901",
            NIMBLE_PROPERTY::READ,
            20
        );
    pDesc->setValue("Face Control");

    //nimBLEService* pBaadService = pServer->createService("BAAD");
    pService->start();

    /** Create an advertising instance and add the services to the advertised data */
    NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->setName("LumiFur_Controller");
    pAdvertising->addServiceUUID(pService->getUUID());
    pAdvertising->enableScanResponse(true);
    pAdvertising->start();

    Serial.println("BLE setup complete - advertising started");


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
dma_display->flipDMABuffer();

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
  statusPixel.begin(); // Initialize NeoPixel status light
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

void displayCurrentMaw() {
  switch (currentMaw) {
    case 0:
  //dma_display->clearScreen();
  Serial.println("Displaying Closed maw");
  drawXbm565(0, 10, 64, 22, maw2Closed, dma_display->color565(255, 255, 255));
  drawXbm565(64, 10, 64, 22, maw2ClosedL, dma_display->color565(255, 255, 255));
  //dma_display->flipDMABuffer();
  break;

  case 1:
  //dma_display->clearScreen();
  Serial.println("Displaying open maw");
  drawXbm565(0, 10, 64, 22, maw2, dma_display->color565(255, 255, 255));
  drawXbm565(64, 10, 64, 22, maw2L, dma_display->color565(255, 255, 255));
  break;

  default:
  Serial.println("Unknown Maw State");
  break;
}
}
void displayCurrentView(int view) {
  static unsigned long lastFrameTime = 0; // Track the last frame time
  const unsigned long frameInterval = 11; // Consistent 30 FPS
  static int previousView = -1; // Track the last active view

   if (millis() - lastFrameTime < frameInterval) return; // Skip frame if not time yet
  lastFrameTime = millis(); // Update last frame time

  dma_display->clearScreen(); // Clear the display

if (view != previousView) { // Check if the view has changed
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
  //dma_display->clearScreen();
    displayLoadingBar();
    if (loadingProgress <= loadingMax)
    {
      displayLoadingBar(); // Update the loading bar
      loadingProgress++;   // Increment progress
      //delay(50);           // Adjust speed of loading animation
    } else {
      // Reset or transition to another view
      loadingProgress = 0;
    }
    //dma_display->flipDMABuffer();
    break;

  case 2: // Pattern plasma
    patternPlasma();
    //delay(10);
    //dma_display->flipDMABuffer();
    break;

  case 3:
    //dma_display->clearScreen(); // Clear the display
    drawTransFlag();
    //delay(20);
    //dma_display->flipDMABuffer();
    break;

  case 4: // Normal
    protoFaceTest();
    updatePlasmaFace();
    updateBlinkAnimation(); // Update blink animation progress
    //delay(20);              // Short delay for smoother animation
    break;

  case 5: // Blush with fade in effect
    protoFaceTest();
    updatePlasmaFace();
    updateBlinkAnimation(); // Update blink animation progress
    break;

  case 6: // Dialated pupils
    protoFaceTest();
    updatePlasmaFace();
    updateBlinkAnimation(); // Update blink animation progress
    break;

  case 7: // X eyes
    protoFaceTest();
    updatePlasmaFace();
    updateBlinkAnimation(); // Update blink animation progress
    break;

  case 8: // Slant eyes
    protoFaceTest();
    updatePlasmaFace();
    updateBlinkAnimation(); // Update blink animation progress
    break;

  case 9: // Spiral eyes
    protoFaceTest();
    updateRotatingSpiral();
//  updateBlinkAnimation(); // Update blink animation progress
    break;

  case 10: //Plasma bitmap test
    drawPlasmaFace();
    updatePlasmaFace();
    updateBlinkAnimation();
    break;
/*
  case 10: // UwU eyes
    protoFaceTest();
    updateBlinkAnimation(); // Update blink animation progress
    break;
  
*/
  default:
      // Optional: Handle unsupported views
      //dma_display->clearScreen();
      break;
  }

  // Only flip buffer if not spiral view (spiral handles its own flip)
  //if (view != 9) 
  dma_display->flipDMABuffer();
}


void loop(void) {
  
  bool isConnected = NimBLEDevice::getServer()->getConnectedCount() > 0;
  
  // Always handle BLE and temperature updates first
  handleBLEConnection();
  updateTemperature();

  // Handle view changes from any source (BLE/buttons)
  static int lastView = -1;
  bool viewChanged = false;

  // Check button presses (outside frame timing)
  if (debounceButton(BUTTON_UP)) {
  currentView = (currentView + 1) % totalViews;
  viewChanged = true;
  }
  if (debounceButton(BUTTON_DOWN)) {
  currentView = (currentView - 1 + totalViews) % totalViews;
  viewChanged = true;
  }

  if(viewChanged && isConnected) {
  uint8_t viewValue = static_cast<uint8_t>(currentView);
  pFaceCharacteristic->setValue(&viewValue, 1);
  pFaceCharacteristic->notify();
  }

  // Frame rate controlled updates
  static unsigned long lastFrameTime = 0;
  const unsigned long frameInterval = 11; // ~90 FPS
  
  if(millis() - lastFrameTime >= frameInterval) {
  lastFrameTime = millis();
  
  // Only update if view needs animation refresh
  switch(currentView) {
    case 0:  // Scrolling text
    case 1:  // Loading bar
    case 2:  // Plasma
    case 4:  // Normal face
    case 5:  // Blush face
    case 9:  // Spiral eyes
    case 10: // Plasma test
    displayCurrentView(currentView);
    break;
  }
  }
}