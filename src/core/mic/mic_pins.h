#ifndef MIC_PINS_H
#define MIC_PINS_H

#if defined(ARDUINO_ADAFRUIT_MATRIXPORTAL_ESP32S3)
#define MIC_SCK_PIN A1 // bit clock
#define MIC_WS_PIN A2  // word-select / LRCLK
#define MIC_SD_PIN A3  // data in
#define MIC_PD_PIN A4  // mic power (optional)
#endif

#endif // MIC_PINS_H
