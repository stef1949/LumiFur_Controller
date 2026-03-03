#include "customEffects/basicRainbow.h"

#include <Arduino.h>
#include <FastLED.h>
#include <cmath>
#include <new>

#ifdef VIRTUAL_PANE
#include <ESP32-VirtualMatrixPanel-I2S-DMA.h>
extern VirtualMatrixPanel *matrix;
#else
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
extern MatrixPanel_I2S_DMA *dma_display;
#endif

extern uint16_t globalBrightnessScaleFixed;

namespace
{
#ifdef VIRTUAL_PANE
VirtualMatrixPanel *getRainbowDisplay()
{
  return matrix;
}
#else
MatrixPanel_I2S_DMA *getRainbowDisplay()
{
  return dma_display;
}
#endif
} // namespace

void patternRainbowGradient()
{
  static uint8_t *angleHueMap = nullptr;
  static size_t cachedPixelCount = 0;
  static int cachedWidth = 0;
  static int cachedHeight = 0;
  static uint16_t hueTo565[256];
  static uint16_t cachedBrightnessScale = 0;

  auto *display = getRainbowDisplay();
  if (!display)
  {
    return;
  }

  const int displayWidth = display->width();
  const int displayHeight = display->height();
  if (displayWidth <= 0 || displayHeight <= 0)
  {
    return;
  }

  const size_t pixelCount = static_cast<size_t>(displayWidth) * static_cast<size_t>(displayHeight);
  if (cachedPixelCount != pixelCount)
  {
    delete[] angleHueMap;
    angleHueMap = new (std::nothrow) uint8_t[pixelCount];
    if (!angleHueMap)
    {
      cachedPixelCount = 0;
      cachedWidth = 0;
      cachedHeight = 0;
      return;
    }
    cachedPixelCount = pixelCount;
    cachedWidth = 0;
    cachedHeight = 0;
  }

  if (cachedWidth != displayWidth || cachedHeight != displayHeight)
  {
    const float centerX = (displayWidth - 1) * 0.5f;
    const float centerY = (displayHeight - 1) * 0.5f;
    const float angleToHue = 255.0f / TWO_PI;

    size_t index = 0;
    for (int y = 0; y < displayHeight; ++y)
    {
      const float dy = static_cast<float>(y) - centerY;
      for (int x = 0; x < displayWidth; ++x)
      {
        const float dx = static_cast<float>(x) - centerX;
        float angle = atan2f(dy, dx);
        if (angle < 0.0f)
        {
          angle += TWO_PI;
        }
        angleHueMap[index++] = static_cast<uint8_t>(angle * angleToHue);
      }
    }

    cachedWidth = displayWidth;
    cachedHeight = displayHeight;
  }

  if (cachedBrightnessScale != globalBrightnessScaleFixed)
  {
    const uint16_t scale = globalBrightnessScaleFixed;
    for (int i = 0; i < 256; ++i)
    {
      CRGB rgb;
      hsv2rgb_rainbow(CHSV(static_cast<uint8_t>(i), 255, 255), rgb);
      rgb.r = static_cast<uint8_t>((static_cast<uint16_t>(rgb.r) * scale + 128) >> 8);
      rgb.g = static_cast<uint8_t>((static_cast<uint16_t>(rgb.g) * scale + 128) >> 8);
      rgb.b = static_cast<uint8_t>((static_cast<uint16_t>(rgb.b) * scale + 128) >> 8);
      hueTo565[i] = display->color565(rgb.r, rgb.g, rgb.b);
    }
    cachedBrightnessScale = scale;
  }

  const uint8_t spinOffset = static_cast<uint8_t>((millis() * 256UL) / 2800UL);

  size_t index = 0;
  for (int y = 0; y < displayHeight; ++y)
  {
    for (int x = 0; x < displayWidth; ++x)
    {
      const uint8_t hue = static_cast<uint8_t>(angleHueMap[index++] + spinOffset);
      display->drawPixel(x, y, hueTo565[hue]);
    }
  }
}
