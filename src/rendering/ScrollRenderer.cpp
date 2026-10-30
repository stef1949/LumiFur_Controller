#include "rendering/RenderSupport.h"
#include "rendering/ScrollRenderer.h"

namespace { int16_t textX=0, textY=0, textMin=0; }
char txt[64];
constexpr uint16_t SCROLL_BACKGROUND_INTERVAL_MS = 90;
constexpr int16_t SCROLL_TEXT_GAP = 12;

void drawText(int colorWheelOffset)
{
  // Set text properties
  dma_display->setFont(&FreeSans9pt7b);
  dma_display->setTextSize(1);

  // Use the color wheel for text color
  uint16_t textColor = colorWheel(colorWheelOffset);
  dma_display->setTextColor(textColor);

  // Set cursor and print text
  dma_display->setCursor(textX, textY);
  dma_display->print(txt);
}

void initializeScrollingText()
{
  if (!dma_display)
  {
    return;
  }

  dma_display->setFont(&FreeSans9pt7b);
  dma_display->setTextSize(1);
  dma_display->setTextWrap(false);

  int16_t x1 = 0;
  int16_t y1 = 0;
  uint16_t w = 0;
  uint16_t h = 0;
  dma_display->getTextBounds(txt, 0, 0, &x1, &y1, &w, &h);

  textX = dma_display->width();
  textY = ((dma_display->height() - h) / 2) - y1;
  textMin = -static_cast<int16_t>(w + SCROLL_TEXT_GAP);

  auto &scrollState = scroll::state();
  scroll::resetTiming();
  const unsigned long now = millis();
  scrollState.lastScrollTickMs = now;
  scrollState.lastBackgroundTickMs = now;
  scrollState.backgroundOffset = 0;
  scrollState.colorOffset = 0;
  scrollState.textInitialized = true;
}

void drawScrollingBackground(uint8_t offset)
{
  if (!dma_display)
  {
    return;
  }

  const int width = dma_display->width();
  const int height = dma_display->height();

  dma_display->clearScreen();
  for (int y = 0; y < height; ++y)
  {
    const uint8_t paletteIndex = sin8(static_cast<uint8_t>(y * 8) + offset);
    const CRGB color = ColorFromPalette(CloudColors_p, paletteIndex);
    dma_display->drawFastHLine(0, y, width, dma_display->color565(color.r, color.g, color.b));
  }
}

void renderScrollingTextView()
{
  auto &scrollState = scroll::state();
  if (!scrollState.textInitialized)
  {
    initializeScrollingText();
  }

  const unsigned long now = millis();

  if (now - scrollState.lastBackgroundTickMs >= SCROLL_BACKGROUND_INTERVAL_MS)
  {
    scrollState.lastBackgroundTickMs = now;
    ++scrollState.backgroundOffset;
  }

  // drawScrollingBackground(scrollState.backgroundOffset);

  if (now - scrollState.lastScrollTickMs >= scrollState.textIntervalMs)
  {
    scrollState.lastScrollTickMs = now;
    --textX;
    if (textX <= textMin)
    {
      textX = dma_display->width() + SCROLL_TEXT_GAP;
    }
    scrollState.colorOffset = static_cast<uint8_t>(scrollState.colorOffset + 3);
  }

  drawText(scrollState.colorOffset);
}
