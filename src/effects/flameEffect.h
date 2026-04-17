#ifndef FLAME_EFFECT_H
#define FLAME_EFFECT_H

#include <Arduino.h>
#include <FastLED.h>

#ifdef VIRTUAL_PANE
#include <ESP32-VirtualMatrixPanel-I2S-DMA.h>
typedef VirtualMatrixPanel DisplayOutputType;
#else
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
typedef MatrixPanel_I2S_DMA DisplayOutputType;
#endif

void initFlameEffect(DisplayOutputType *display);

void updateAndDrawFlameEffect();

#endif // FLAME_EFFECT_H
