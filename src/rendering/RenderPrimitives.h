#pragma once
#include <cstdint>

void drawXbm565(int x, int y, int width, int height, const uint8_t *xbm, uint16_t color = 0xffff);
uint16_t colorWheel(uint8_t pos);
void drawPlasmaXbm(int x, int y, int width, int height, const uint8_t *xbm,
                   uint8_t time_offset = 0, float scale = 5.0f, float animSpeed = 0.2f,
                   uint8_t brightnessScale = 255, bool bypassGlobalBrightness = false);
void drawBitmapWithBlink(int x, int y, int width, int height, const uint8_t *bitmap, uint16_t color, int progress);
void drawBitmapAdvanced(int x, int y, int width, int height, const uint8_t *bitmap,
                        uint16_t color, int progress, bool usePlasma,
                        uint8_t time_offset = 0, float scale = 5.0, float animSpeed = 0.2f);
void drawRotatedBitmap(int16_t x, int16_t y, const uint8_t *bitmap,
                       uint16_t w, uint16_t h, float cosA, float sinA,
                       uint16_t color);
