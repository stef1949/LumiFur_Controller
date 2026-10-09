#ifndef MATRIX_RAIN_EFFECT_H
#define MATRIX_RAIN_EFFECT_H

#include <Arduino.h>

#ifdef VIRTUAL_PANE
#include <ESP32-VirtualMatrixPanel-I2S-DMA.h>
typedef VirtualMatrixPanel DisplayOutputType;
#else
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
typedef MatrixPanel_I2S_DMA DisplayOutputType;
#endif

void initMatrixRainEffect(DisplayOutputType *display);

// Face 1: full-panel Matrix rain
void updateAndDrawMatrixRainEffect();

// Face 2 support: advance sim + stencil helpers
void matrixRainAdvance();
uint16_t matrixRainColorAt(int x, int y);
void drawMatrixRainThroughXbm(int x, int y, int width, int height,
                              const uint8_t *xbm);

#endif