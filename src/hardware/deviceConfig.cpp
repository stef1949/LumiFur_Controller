#include "hardware/deviceConfig.h"

#if defined(ARDUINO_ADAFRUIT_MATRIXPORTAL_ESP32S3_Waveshare)
Adafruit_APDS9960 apds;
Adafruit_NeoPixel statusPixel(1, STATUS_LED_PIN, NEO_GRB + NEO_KHZ800);
#elif defined(ARDUINO_ADAFRUIT_MATRIXPORTAL_ESP32S3)
Adafruit_APDS9960 apds;
Adafruit_NeoPixel statusPixel(1, STATUS_LED_PIN, NEO_GRB + NEO_KHZ800);
#endif

#ifdef VIRTUAL_PANE
VirtualMatrixPanel *matrix = nullptr;
MatrixPanel_I2S_DMA *chain = nullptr;
#else
MatrixPanel_I2S_DMA *dma_display = nullptr;
#endif

CRGB leds[1];

unsigned long t1 = 0;
unsigned long t2 = 0;
unsigned long s1 = 0;
unsigned long s2 = 0;
unsigned long s3 = 0;
uint32_t ccount1 = 0;
uint32_t ccount2 = 0;

uint8_t color1 = 0;
uint8_t color2 = 0;
uint8_t color3 = 0;
uint16_t x = 0;
uint16_t y = 0;

const char *message = "* ESP32 I2S DMA *";
