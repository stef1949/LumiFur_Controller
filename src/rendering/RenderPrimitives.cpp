#include "rendering/RenderSupport.h"
#include "rendering/RenderPrimitives.h"

static inline void drawPixelRgbFast(int x, int y, uint8_t r, uint8_t g, uint8_t b)
{
  // Keep all eight channel bits, including on virtual panels, so dim colours
  // are not truncated to black by an intermediate RGB565 conversion.
  dma_display->drawPixelRGB888(x, y, r, g, b);
}

struct ScaledPlasmaPaletteCache
{
  bool valid = false;
  uint16_t brightnessScale = 0;
  uint32_t stamp = 0;
  CRGBPalette16 palette;
  CRGB entries[256];
};

static ScaledPlasmaPaletteCache gScaledPlasmaPaletteCaches[2];
static uint32_t gScaledPlasmaPaletteCacheStamp = 0;

static const CRGB *getScaledPlasmaPaletteLut(uint16_t brightnessScale)
{
  ++gScaledPlasmaPaletteCacheStamp;
  ScaledPlasmaPaletteCache *selected = &gScaledPlasmaPaletteCaches[0];

  for (ScaledPlasmaPaletteCache &cache : gScaledPlasmaPaletteCaches)
  {
    if (cache.valid &&
        cache.brightnessScale == brightnessScale &&
        std::memcmp(&cache.palette, &currentPalette, sizeof(CRGBPalette16)) == 0)
    {
      cache.stamp = gScaledPlasmaPaletteCacheStamp;
      return cache.entries;
    }

    if (!cache.valid || cache.stamp < selected->stamp)
    {
      selected = &cache;
    }
  }

  selected->valid = true;
  selected->brightnessScale = brightnessScale;
  selected->palette = currentPalette;
  selected->stamp = gScaledPlasmaPaletteCacheStamp;

  for (int i = 0; i < 256; ++i)
  {
    CRGB color = ColorFromPalette(currentPalette, static_cast<uint8_t>(i));
    color.r = static_cast<uint8_t>((static_cast<uint16_t>(color.r) * brightnessScale + 128) >> 8);
    color.g = static_cast<uint8_t>((static_cast<uint16_t>(color.g) * brightnessScale + 128) >> 8);
    color.b = static_cast<uint8_t>((static_cast<uint16_t>(color.b) * brightnessScale + 128) >> 8);
    selected->entries[i] = color;
  }

  return selected->entries;
}

void drawXbm565(int x, int y, int width, int height, const uint8_t *xbm, uint16_t color)
{
  // Ensure width is padded to the nearest byte boundary
  int byteWidth = (width + 7) / 8;

  // Pre-check if entire bitmap is out of bounds
  if (x >= dma_display->width() || y >= dma_display->height() ||
      x + width <= 0 || y + height <= 0)
  {
    return; // Completely out of bounds, nothing to draw
  }

  // Calculate visible region to avoid per-pixel boundary checks
  // Use explicit conditionals instead of min/max to avoid overload or macro conflicts.
  int startX = (0 > -x) ? 0 : -x;
  int startY = (0 > -y) ? 0 : -y;
  int tmpEndX = dma_display->width() - x;
  int endX = (width < tmpEndX) ? width : tmpEndX;
  int tmpEndY = dma_display->height() - y;
  int endY = (height < tmpEndY) ? height : tmpEndY;

  for (int j = startY; j < endY; j++)
  {
    uint8_t bitMask = 0x80 >> (startX & 7);        // Start with the correct bit position
    int byteIndex = j * byteWidth + (startX >> 3); // Integer division by 8

    for (int i = startX; i < endX; i++)
    {
      // Check if the bit is set
      if (pgm_read_byte(&xbm[byteIndex]) & bitMask)
      {
        dma_display->drawPixel(x + i, y + j, color);
      }

      // Move to the next bit
      bitMask >>= 1;
      if (bitMask == 0)
      {                 // We've used all bits in this byte
        bitMask = 0x80; // Reset to the first bit of the next byte
        byteIndex++;    // Move to the next byte
      }
    }
  }
}

uint16_t colorWheel(uint8_t pos)
{
  if (pos < 85)
  {
    return dma_display->color565(pos * 3, 255 - pos * 3, 0);
  }
  else if (pos < 170)
  {
    pos -= 85;
    return dma_display->color565(255 - pos * 3, 0, pos * 3);
  }
  pos -= 170;
  return dma_display->color565(0, pos * 3, 255 - pos * 3);
}

void drawPlasmaXbm(int x, int y, int width, int height, const uint8_t *xbm,
                   uint8_t time_offset, float scale, float animSpeed,
                   uint8_t brightnessScale, bool bypassGlobalBrightness)
{

  const int byteWidth = (width + 7) >> 3;
  if (byteWidth <= 0)
  {
    return;
  }

  const bool useStaticColorMode = staticColorModeEnabled;
  const uint16_t combinedBrightnessScale = bypassGlobalBrightness
                                               ? micBrightnessToFixedScale(brightnessScale)
                                               : static_cast<uint16_t>(
                                                     (static_cast<uint32_t>(getNormalFaceSoftwareBrightnessScale()) *
                                                          micBrightnessToFixedScale(brightnessScale) +
                                                      128u) >>
                                                     8);
  if (matrixRainInsteadOfPlasma)
  {
    matrixRainAdvanceColumns();
    drawMatrixRainThroughXbm(x, y, width, height, xbm, combinedBrightnessScale);
    return;
  }

  CRGB staticColorRgb = CRGB::Black;
  if (useStaticColorMode)
  {
    ensureStaticColorLoaded();
    staticColorRgb = getStaticColorCached();
    staticColorRgb.r = static_cast<uint8_t>((static_cast<uint16_t>(staticColorRgb.r) * combinedBrightnessScale + 128) >> 8);
    staticColorRgb.g = static_cast<uint8_t>((static_cast<uint16_t>(staticColorRgb.g) * combinedBrightnessScale + 128) >> 8);
    staticColorRgb.b = static_cast<uint8_t>((static_cast<uint16_t>(staticColorRgb.b) * combinedBrightnessScale + 128) >> 8);
  }

  const uint16_t scaleFixed = static_cast<uint16_t>(scale * 256.0f);
  if (scaleFixed == 0)
  {
    return;
  }
  const uint16_t scaleHalfFixed = scaleFixed >> 1;
  int32_t startXFixed = static_cast<int32_t>(x) * static_cast<int32_t>(scaleFixed);

  const uint16_t animSpeedFixed = static_cast<uint16_t>(animSpeed * 256.0f);
  const uint32_t effectiveTimeFixed = static_cast<uint32_t>(time_counter) * animSpeedFixed;
  const uint8_t t = static_cast<uint8_t>(effectiveTimeFixed >> 8);
  const uint8_t t2 = static_cast<uint8_t>(t >> 1);
  const uint8_t t3 = static_cast<uint8_t>(((effectiveTimeFixed / 3U) >> 8) & 0xFF);

  const uint16_t brightnessScaleFixed = combinedBrightnessScale;
  const CRGB *paletteLut = useStaticColorMode ? nullptr : getScaledPlasmaPaletteLut(brightnessScaleFixed);

  for (int j = 0; j < height; ++j)
  {
    const int yj = y + j;
    const int32_t yScaleFixed = static_cast<int32_t>(yj) * scaleFixed;
    const uint8_t cos_val = cos8(static_cast<uint8_t>((yScaleFixed >> 8) + t2));
    int32_t tempFixed = static_cast<int32_t>(x + yj) * scaleHalfFixed;
    int32_t xFixed = startXFixed;
    const uint8_t *rowPtr = reinterpret_cast<const uint8_t *>(xbm) + j * byteWidth;

    for (int byteIndex = 0; byteIndex < byteWidth; ++byteIndex)
    {
      const uint8_t rowBits = rowPtr[byteIndex];
      const int pixelBase = byteIndex << 3;
      int pixelsInByte = width - pixelBase;
      if (pixelsInByte > 8)
      {
        pixelsInByte = 8;
      }
      if (pixelsInByte <= 0)
      {
        break;
      }

      if (rowBits == 0)
      {
        xFixed += static_cast<int32_t>(scaleFixed) * pixelsInByte;
        tempFixed += static_cast<int32_t>(scaleHalfFixed) * pixelsInByte;
        continue;
      }

      for (int bitIndex = 0; bitIndex < pixelsInByte; ++bitIndex)
      {
        if (rowBits & static_cast<uint8_t>(0x80U >> bitIndex))
        {
          const int pixelX = x + pixelBase + bitIndex;
          if (useStaticColorMode)
          {
            drawPixelRgbFast(pixelX, yj, staticColorRgb.r, staticColorRgb.g, staticColorRgb.b);
          }
          else
          {
            const uint8_t sin_val = sin8(static_cast<uint8_t>((xFixed >> 8) + t));
            const uint8_t sin_val2 = sin8(static_cast<uint8_t>((tempFixed >> 8) + t3));
            const uint8_t v = sin_val + cos_val + sin_val2;

            const uint8_t paletteIndex = static_cast<uint8_t>(v + time_offset);
            const CRGB &color = paletteLut[paletteIndex];

            drawPixelRgbFast(pixelX, yj, color.r, color.g, color.b);
          }
        }

        xFixed += scaleFixed;
        tempFixed += scaleHalfFixed;
      }
    }
  }
}

void drawBitmapWithBlink(int x, int y, int width, int height, const uint8_t *bitmap, uint16_t color, int progress)
{
  int byteWidth = (width + 7) / 8;
  float center_y = (height - 1) / 2.0f;

  // This formula replicates the "w" calculation from the emulator for the squash effect
  float w = 0.005f + (1.0f - 0.005f) * (progress / 100.0f);

  for (int j = 0; j < height; j++)
  {
    for (int i = 0; i < width; i++)
    {
      if (pgm_read_byte(&bitmap[j * byteWidth + i / 8]) & (0x80 >> (i % 8)))
      {
        // Calculate brightness based on vertical distance from center, modulated by 'w'
        float blinkBrightness = pow(2, -w * pow(j - center_y, 2));
        if (blinkBrightness < 0.01)
          continue;

        // Apply the main color modulated by the blink brightness
        uint8_t r = (color >> 11) & 0x1F;
        uint8_t g = (color >> 5) & 0x3F;
        uint8_t b = color & 0x1F;

        r = r * blinkBrightness;
        g = g * blinkBrightness;
        b = b * blinkBrightness;

        dma_display->drawPixel(x + i, y + j, dma_display->color565(r << 3, g << 2, b << 3));
      }
    }
  }
}

void drawBitmapAdvanced(int x, int y, int width, int height, const uint8_t *bitmap,
                        uint16_t color, int progress, bool usePlasma,
                        uint8_t time_offset, float scale, float animSpeed)
{
  const bool enableMatrix = matrixRainInsteadOfPlasma && usePlasma;
  if (enableMatrix)
  {
    matrixRainAdvanceColumns();
  }
  const int byteWidth = (width + 7) / 8;
  const float center_y = (height - 1) / 2.0f;

  // --- Blink Effect Calculation ---
  const float w = 0.005f + (1.0f - 0.005f) * (progress / 100.0f);

  const bool enablePlasma = usePlasma && !staticColorModeEnabled && !enableMatrix;
  CRGB staticColorValue;
  if (staticColorModeEnabled)
  {
    ensureStaticColorLoaded();
    staticColorValue = getStaticColorCached();
  }
  else
  {
    const uint8_t r = static_cast<uint8_t>((((color >> 11) & 0x1F) * 255) / 31);
    const uint8_t g = static_cast<uint8_t>((((color >> 5) & 0x3F) * 255) / 63);
    const uint8_t b = static_cast<uint8_t>(((color & 0x1F) * 255) / 31);
    staticColorValue = CRGB(r, g, b);
  }

  // --- Plasma Effect Setup (if enabled) ---
  uint8_t t = 0, t2 = 0, t3 = 0;
  float scaleHalf = 0;
  if (enablePlasma)
  {
    float effectiveTimeFloat = time_counter * animSpeed;
    t = (uint8_t)effectiveTimeFloat;
    t2 = (uint8_t)(effectiveTimeFloat / 2);
    t3 = (uint8_t)(effectiveTimeFloat / 3);
    scaleHalf = scale * 0.5f;
  }

  for (int j = 0; j < height; j++)
  {
    // --- OPTIMIZATION: Calculate blink brightness once per row ---
    const float blinkBrightness = powf(2.0f, -w * powf(j - center_y, 2));
    // If the entire row is too dim to be visible, skip it completely.
    if (blinkBrightness < 0.01f)
    {
      continue;
    }

    const uint16_t rowBrightnessScale = static_cast<uint16_t>(
        blinkBrightness * getNormalFaceSoftwareBrightnessScale());
    CRGB flatRowColor = CRGB::Black;
    if (!enablePlasma)
    {
      flatRowColor = staticColorValue;
      flatRowColor.r = static_cast<uint8_t>((static_cast<uint16_t>(flatRowColor.r) * rowBrightnessScale + 128) >> 8);
      flatRowColor.g = static_cast<uint8_t>((static_cast<uint16_t>(flatRowColor.g) * rowBrightnessScale + 128) >> 8);
      flatRowColor.b = static_cast<uint8_t>((static_cast<uint16_t>(flatRowColor.b) * rowBrightnessScale + 128) >> 8);
    }

    // Pre-calculate plasma values that are constant for the row
    const float y_val_plasma = (y + j) * scale;
    float temp_val_plasma = (x + y + j) * scaleHalf;
    float x_val_plasma = x * scale;
    uint8_t cos_val_plasma = 0;
    if (enablePlasma)
    {
      cos_val_plasma = cos8(y_val_plasma + t2);
    }

    const uint8_t *rowPtr = bitmap + (j * byteWidth);
    for (int byteIndex = 0; byteIndex < byteWidth; ++byteIndex)
    {
      const uint8_t rowBits = pgm_read_byte(rowPtr + byteIndex);
      const int pixelBase = byteIndex << 3;
      int pixelsInByte = width - pixelBase;
      if (pixelsInByte > 8)
      {
        pixelsInByte = 8;
      }
      if (pixelsInByte <= 0)
      {
        break;
      }

      if (rowBits == 0)
      {
        if (enablePlasma)
        {
          x_val_plasma += scale * pixelsInByte;
          temp_val_plasma += scaleHalf * pixelsInByte;
        }
        continue;
      }

      for (int bitIndex = 0; bitIndex < pixelsInByte; ++bitIndex)
      {
        if (rowBits & static_cast<uint8_t>(0x80U >> bitIndex))
        {
          const int pixelX = x + pixelBase + bitIndex;
          if (enableMatrix)
          {
            dma_display->drawPixel(pixelX, y + j,
                                   matrixRainColorAt(pixelX, y + j, rowBrightnessScale));
          }
          else if (enablePlasma)
          {
            // --- Plasma Color Calculation ---
            uint8_t sin_val = sin8(x_val_plasma + t);
            uint8_t sin_val2 = sin8(temp_val_plasma + t3);
            uint8_t v = sin_val + cos_val_plasma + sin_val2;
            CRGB final_color = ColorFromPalette(currentPalette, v + time_offset);
            final_color.r = static_cast<uint8_t>((static_cast<uint16_t>(final_color.r) * rowBrightnessScale + 128) >> 8);
            final_color.g = static_cast<uint8_t>((static_cast<uint16_t>(final_color.g) * rowBrightnessScale + 128) >> 8);
            final_color.b = static_cast<uint8_t>((static_cast<uint16_t>(final_color.b) * rowBrightnessScale + 128) >> 8);
            drawPixelRgbFast(pixelX, y + j, final_color.r, final_color.g, final_color.b);
          }
          else
          {
            drawPixelRgbFast(pixelX, y + j, flatRowColor.r, flatRowColor.g, flatRowColor.b);
          }
        }

        if (enablePlasma)
        {
          x_val_plasma += scale;
          temp_val_plasma += scaleHalf;
        }
      }
    }
  }
}

void drawRotatedBitmap(int16_t x, int16_t y, const uint8_t *bitmap,
                       uint16_t w, uint16_t h, float cosA, float sinA,
                       uint16_t color)
{
  const int16_t cx = w / 2;
  const int16_t cy = h / 2;
  const int byteWidth = (w + 7) >> 3;

  const int32_t cosFixed = static_cast<int32_t>(cosA * 65536.0f);
  const int32_t sinFixed = static_cast<int32_t>(sinA * 65536.0f);

  for (uint16_t j = 0; j < h; ++j)
  {
    const int16_t dy = static_cast<int16_t>(j) - cy;
    int32_t baseXFixed = (static_cast<int32_t>(-cx) * cosFixed) - (static_cast<int32_t>(dy) * sinFixed);
    int32_t baseYFixed = (static_cast<int32_t>(-cx) * sinFixed) + (static_cast<int32_t>(dy) * cosFixed);

    baseXFixed += static_cast<int32_t>(x) << 16;
    baseYFixed += static_cast<int32_t>(y) << 16;

    int32_t pixelXFixed = baseXFixed;
    int32_t pixelYFixed = baseYFixed;
    uint8_t currentByte = 0;

    for (uint16_t i = 0; i < w; ++i)
    {
      if ((i & 7U) == 0U)
      {
        currentByte = pgm_read_byte(&bitmap[j * byteWidth + (i >> 3)]);
      }

      if (currentByte & (0x80U >> (i & 7U)))
      {
        const int16_t newX = static_cast<int16_t>(pixelXFixed >> 16);
        const int16_t newY = static_cast<int16_t>(pixelYFixed >> 16);

        if (newX >= 0 && newX < PANE_WIDTH && newY >= 0 && newY < PANE_HEIGHT)
        {
          dma_display->drawPixel(newX, newY, color);
        }
      }

      pixelXFixed += cosFixed;
      pixelYFixed += sinFixed;
    }
  }
}
