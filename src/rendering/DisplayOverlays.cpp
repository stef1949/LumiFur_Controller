#include "rendering/RenderSupport.h"
#include "rendering/DisplayOverlays.h"
#include "ble/BleStatus.h"
#include "assets/bitmaps.h"

constexpr uint8_t BLUETOOTH_BACKGROUND_WIDTH = 7;
constexpr uint8_t BLUETOOTH_BACKGROUND_HEIGHT = 11;
constexpr uint8_t BLUETOOTH_RUNE_WIDTH = 5;
constexpr uint8_t BLUETOOTH_RUNE_HEIGHT = 7;
constexpr uint8_t BLUETOOTH_ICON_MARGIN = 1;
constexpr unsigned long BLUETOOTH_FADE_PERIOD_MS = 1600UL;
constexpr unsigned long BLUETOOTH_CONNECTED_FADE_DELAY_MS = 2000UL;
constexpr unsigned long BLUETOOTH_CONNECTED_FADE_DURATION_MS = 1500UL;

bool bluetoothOverlayNeedsContinuousRefresh(unsigned long nowMs)
{
#if DEBUG_DISABLE_BLE_STATUS_ICON
  (void)nowMs;
  return false;
#else
  if (!deviceConnected)
  {
    return true;
  }

  if (bleStatusChangedAt() == 0)
  {
    return false;
  }

  const unsigned long connectedDuration = nowMs - bleStatusChangedAt();
  return connectedDuration <= (BLUETOOTH_CONNECTED_FADE_DELAY_MS + BLUETOOTH_CONNECTED_FADE_DURATION_MS);
#endif
}

uint8_t scaleColorComponent(uint8_t value, float intensity)
{
  float scaled = value * intensity;
  if (scaled < 0.0f)
  {
    scaled = 0.0f;
  }
  if (scaled > 255.0f)
  {
    scaled = 255.0f;
  }
  return static_cast<uint8_t>(scaled + 0.5f);
}

void drawBluetoothStatusIcon()
{
  // Byte 7. Do not draw the on-panel BLE rune.
if (disableBleStatusIcon)
  {
    return;
  }

  const unsigned long nowMs = millis();
  const int iconX = dma_display->width() - BLUETOOTH_BACKGROUND_WIDTH - BLUETOOTH_ICON_MARGIN - 64;
  const int iconY = dma_display->height() - BLUETOOTH_BACKGROUND_HEIGHT - BLUETOOTH_ICON_MARGIN;

  float intensity = 1.0f;
  if (!deviceConnected)
  {
    const float normalized = static_cast<float>(nowMs % BLUETOOTH_FADE_PERIOD_MS) / static_cast<float>(BLUETOOTH_FADE_PERIOD_MS);
    const float wave = 0.5f * (sinf(normalized * TWO_PI) + 1.0f); // Range 0..1
    intensity = 0.15f + (0.65f * wave);                           // Keep a faint glow at minimum
  }
  else
  {
    if (bleStatusChangedAt() == 0)
    {
      ensureBleStatusTimestamp(nowMs);
    }
    const unsigned long connectedDuration = nowMs - bleStatusChangedAt();
    if (connectedDuration > BLUETOOTH_CONNECTED_FADE_DELAY_MS)
    {
      const unsigned long fadeElapsed = connectedDuration - BLUETOOTH_CONNECTED_FADE_DELAY_MS;
      if (fadeElapsed >= BLUETOOTH_CONNECTED_FADE_DURATION_MS)
      {
        intensity = 0.0f;
      }
      else
      {
        const float fadeProgress = static_cast<float>(fadeElapsed) / static_cast<float>(BLUETOOTH_CONNECTED_FADE_DURATION_MS);
        intensity = 1.0f - fadeProgress;
      }
    }
  }

  // After fade-out completes, stop drawing the connected icon in normal mode.
  if (deviceConnected && !sleepModeActive && intensity <= 0.0f)
  {
    return;
  }

  const uint16_t backgroundColor = dma_display->color565(
      scaleRawFaceComponentForPanelHeadroom(scaleColorComponent(5, intensity)),
      scaleRawFaceComponentForPanelHeadroom(scaleColorComponent(90, intensity)),
      scaleRawFaceComponentForPanelHeadroom(scaleColorComponent(180, intensity)));

  // NEW: Brighter constant for ble pixel color
  const uint16_t blePixelColor = dma_display->color565(
      scaleRawFaceComponentForPanelHeadroom(scaleColorComponent(5, intensity + 0.8f)),
      scaleRawFaceComponentForPanelHeadroom(scaleColorComponent(90, intensity + 0.8f)),
      scaleRawFaceComponentForPanelHeadroom(scaleColorComponent(180, intensity + 0.8f)));

  const uint16_t runeColor = dma_display->color565(
      scaleRawFaceComponentForPanelHeadroom(scaleColorComponent(255, intensity)),
      scaleRawFaceComponentForPanelHeadroom(scaleColorComponent(255, intensity)),
      scaleRawFaceComponentForPanelHeadroom(scaleColorComponent(255, intensity)));

  // Clear the drawing area (pad by 1 px to avoid leftover pixels from other views)
  int clearX = iconX - 1;
  int clearY = iconY - 1;
  int clearW = BLUETOOTH_BACKGROUND_WIDTH + 2;
  int clearH = BLUETOOTH_BACKGROUND_HEIGHT + 2;
  if (clearX < 0)
  {
    clearW += clearX;
    clearX = 0;
  }
  if (clearY < 0)
  {
    clearH += clearY;
    clearY = 0;
  }
  if (clearW > 0 && clearH > 0)
  {
    // dma_display->fillRect(clearX, clearY, clearW, clearH, 0);
  }
  if (!sleepModeActive)
  {
    drawXbm565(iconX, iconY, BLUETOOTH_BACKGROUND_WIDTH, BLUETOOTH_BACKGROUND_HEIGHT, bluetoothBackground, backgroundColor);
    drawXbm565(iconX + BLUETOOTH_BACKGROUND_WIDTH + 2, iconY, BLUETOOTH_BACKGROUND_WIDTH, BLUETOOTH_BACKGROUND_HEIGHT, bluetoothBackground, backgroundColor);

    const int runeX = iconX + static_cast<int>((BLUETOOTH_BACKGROUND_WIDTH - BLUETOOTH_RUNE_WIDTH) / 2);
    const int runeY = iconY + static_cast<int>((BLUETOOTH_BACKGROUND_HEIGHT - BLUETOOTH_RUNE_HEIGHT) / 2);

    drawXbm565(runeX, runeY, BLUETOOTH_RUNE_WIDTH, BLUETOOTH_RUNE_HEIGHT, bluetoothRune, runeColor);
    drawXbm565(runeX + BLUETOOTH_RUNE_WIDTH + 4, runeY, BLUETOOTH_RUNE_WIDTH, BLUETOOTH_RUNE_HEIGHT, bluetoothRune, runeColor);
  }
  else
  {
    dma_display->drawPixel(63, 0, blePixelColor);
    dma_display->drawPixel(64, 0, blePixelColor);
  }
}

void drawPairingPasskeyOverlay(uint32_t passkey)
{
  if (!dma_display)
  {
    return;
  }

  static bool labelMetricsCached = false;
  static int16_t labelX1 = 0;
  static int16_t labelY1 = 0;
  static uint16_t labelW = 0;
  static uint16_t labelH = 0;
  static bool keyMetricsCached = false;
  static uint32_t cachedPasskey = 0;
  static int16_t keyX1 = 0;
  static int16_t keyY1 = 0;
  static uint16_t keyW = 0;
  static uint16_t keyH = 0;

  char passkeyStr[7];
  snprintf(passkeyStr, sizeof(passkeyStr), "%06lu", static_cast<unsigned long>(passkey));

  const char *label = "PASSKEY";

  if (!labelMetricsCached)
  {
    dma_display->setFont(&TomThumb);
    dma_display->setTextSize(1);
    dma_display->getTextBounds(label, 0, 0, &labelX1, &labelY1, &labelW, &labelH);
    labelMetricsCached = true;
  }

  if (!keyMetricsCached || passkey != cachedPasskey)
  {
    dma_display->setFont(&FreeSans9pt7b);
    dma_display->setTextSize(1);
    dma_display->getTextBounds(passkeyStr, 0, 0, &keyX1, &keyY1, &keyW, &keyH);
    cachedPasskey = passkey;
    keyMetricsCached = true;
  }

  const int padding = 2;
  const int gap = 1;
  const int contentW = (labelW > keyW) ? labelW : keyW;
  const int boxW = contentW + (padding * 2);
  const int boxH = labelH + keyH + gap + (padding * 2);

  // Draw the same box centred on each panel half so the code reads from either side.
  const int screenH = dma_display->height();
  const int halfW = dma_display->width() / 2;
  const int drawW = (boxW > halfW) ? halfW : boxW;
  int boxY = (screenH - boxH) / 2;
  if (boxY < 0)
  {
    boxY = 0;
  }

  const uint16_t backgroundColor = dma_display->color565(0, 0, 0);
  const uint16_t borderColor = dma_display->color565(255, 255, 255);

  for (int half = 0; half < 2; ++half)
  {
    const int boxX = (half * halfW) + (halfW - drawW) / 2;

    dma_display->fillRect(boxX, boxY, drawW, boxH, backgroundColor);
    dma_display->drawRect(boxX, boxY, drawW, boxH, borderColor);

    const int labelLeft = boxX + (drawW - labelW) / 2;
    const int labelTop = boxY + padding;

    dma_display->setFont(&TomThumb);
    dma_display->setTextSize(1);
    dma_display->setTextColor(borderColor);
    dma_display->setCursor(labelLeft - labelX1, labelTop - labelY1);
    dma_display->print(label);

    const int keyLeft = boxX + (drawW - keyW) / 2;
    const int keyTop = labelTop + labelH + gap;

    dma_display->setFont(&FreeSans9pt7b);
    dma_display->setTextSize(1);
    dma_display->setTextColor(borderColor);
    dma_display->setCursor(keyLeft - keyX1, keyTop - keyY1);
    dma_display->print(passkeyStr);
  }

}
