#include "effects/flameEffect.h"

#include <new>

extern uint16_t globalBrightnessScaleFixed;

static DisplayOutputType *g_display_ptr = nullptr;
static uint16_t *g_scanline_buffer = nullptr;
static int32_t g_display_width = 0;
static int32_t g_display_height = 0;

namespace
{
  constexpr uint8_t kFireCooling = 8;
  constexpr uint32_t kFireDetail = 6000;
  constexpr uint32_t kFireSpeed = 256;

  CRGB heatMapColor(uint8_t index)
  {
    if (index < 128)
    {
      return CRGB(static_cast<uint8_t>(index << 1), 0, 0);
    }

    if (index < 224)
    {
      return CRGB(255, static_cast<uint8_t>((static_cast<uint16_t>(index - 128) * 8u) / 3u), 0);
    }

    return CRGB(255, 255, static_cast<uint8_t>((index - 224) << 3));
  }

  const uint16_t *getHeatLutForBrightness(DisplayOutputType *display)
  {
    static uint16_t heatTo565[256];
    static uint16_t cachedBrightnessScale = 0;

    if (cachedBrightnessScale != globalBrightnessScaleFixed)
    {
      const uint16_t scale = globalBrightnessScaleFixed;
      for (int i = 0; i < 256; ++i)
      {
        CRGB color = heatMapColor(static_cast<uint8_t>(i));
        color.r = static_cast<uint8_t>((static_cast<uint16_t>(color.r) * scale + 128u) >> 8);
        color.g = static_cast<uint8_t>((static_cast<uint16_t>(color.g) * scale + 128u) >> 8);
        color.b = static_cast<uint8_t>((static_cast<uint16_t>(color.b) * scale + 128u) >> 8);
        heatTo565[i] = display->color565(color.r, color.g, color.b);
      }
      cachedBrightnessScale = scale;
    }

    return heatTo565;
  }

  void freeFlameBuffers()
  {
    if (g_scanline_buffer)
    {
      delete[] g_scanline_buffer;
      g_scanline_buffer = nullptr;
    }
  }
} // namespace

void initFlameEffect(DisplayOutputType *display)
{
  g_display_ptr = display;
  if (!g_display_ptr)
  {
    return;
  }

  g_display_width = (int32_t)g_display_ptr->width();
  g_display_height = (int32_t)g_display_ptr->height();

  if (g_display_width <= 0 || g_display_height <= 0)
  {
    return;
  }

  freeFlameBuffers();

  g_scanline_buffer = new (std::nothrow) uint16_t[g_display_width];

  if (!g_scanline_buffer)
  {
    Serial.println("FlameEffect: RAM Allocation Failed!");
    freeFlameBuffers();
    return;
  }

  memset(g_scanline_buffer, 0, g_display_width * sizeof(uint16_t));

  Serial.printf("Flame initialized: %dx%d\n", g_display_width, g_display_height);
}

void updateAndDrawFlameEffect()
{
  if (!g_display_ptr || !g_scanline_buffer)
  {
    return;
  }

  if ((int32_t)g_display_ptr->width() != g_display_width ||
      (int32_t)g_display_ptr->height() != g_display_height)
  {
    initFlameEffect(g_display_ptr);
    if (!g_scanline_buffer)
    {
      return;
    }
  }

  const uint16_t *heatTo565 = getHeatLutForBrightness(g_display_ptr);
  const uint32_t timeOffset = (micros() >> 10) * kFireSpeed;

  for (int32_t y = 0; y < g_display_height; ++y)
  {
    const uint32_t heightFromBase = static_cast<uint32_t>((g_display_height - 1) - y);
    const uint32_t noiseY = heightFromBase * kFireDetail;
    const uint32_t cooling = heightFromBase * kFireCooling;

    for (int32_t x = 0; x < g_display_width; ++x)
    {
      uint8_t heat = static_cast<uint8_t>(
          inoise16(noiseY - timeOffset, static_cast<uint32_t>(x) * kFireDetail) >> 8);

      if (cooling >= heat)
      {
        heat = 0;
      }
      else
      {
        heat = static_cast<uint8_t>(heat - cooling);
      }

      g_scanline_buffer[x] = heatTo565[heat];
    }

    g_display_ptr->drawRGBBitmap(0, y, g_scanline_buffer, g_display_width, 1);

    if ((y & 0x7) == 0)
    {
      yield();
    }
  }
}
