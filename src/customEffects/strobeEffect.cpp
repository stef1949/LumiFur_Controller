#include "customEffects/strobeEffect.h"

#include <Arduino.h>
#include <cstdlib>
#include <string>

#include "core/ColorParser.h"
#include "userPreferences.h"

#ifdef VIRTUAL_PANE
#include <ESP32-VirtualMatrixPanel-I2S-DMA.h>
extern VirtualMatrixPanel *matrix;
#else
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
extern MatrixPanel_I2S_DMA *dma_display;
#endif

namespace
{
constexpr uint16_t kDefaultStrobeCycleMs = 120;
constexpr uint16_t kMinStrobeCycleMs = 20;
constexpr uint16_t kMaxStrobeCycleMs = 2000;
constexpr uint16_t kMinFlashMs = 10;

struct StrobeSettings
{
  CRGB color = CRGB::White;
  uint16_t cycleMs = kDefaultStrobeCycleMs;
  bool loaded = false;
};

StrobeSettings gStrobeSettings;

#ifdef VIRTUAL_PANE
VirtualMatrixPanel *getStrobeDisplay()
{
  return matrix;
}
#else
MatrixPanel_I2S_DMA *getStrobeDisplay()
{
  return dma_display;
}
#endif

RgbColor toRgbColor(const CRGB &color)
{
  RgbColor result;
  result.r = color.r;
  result.g = color.g;
  result.b = color.b;
  return result;
}

CRGB toCRGB(const RgbColor &color)
{
  return CRGB(color.r, color.g, color.b);
}

uint16_t clampStrobeCycleMs(uint16_t cycleMs)
{
  if (cycleMs < kMinStrobeCycleMs)
  {
    return kMinStrobeCycleMs;
  }
  if (cycleMs > kMaxStrobeCycleMs)
  {
    return kMaxStrobeCycleMs;
  }
  return cycleMs;
}

void setStrobeColorInternal(const CRGB &color)
{
  gStrobeSettings.color = color;
}

void setStrobeCycleInternal(uint16_t cycleMs)
{
  gStrobeSettings.cycleMs = clampStrobeCycleMs(cycleMs);
}

bool loadStrobeColorFromPreferences()
{
  String stored = getStrobeColorPreference();
  if (stored.length() == 0)
  {
    setStrobeColorInternal(CRGB::White);
    return false;
  }

  std::string rawValue;
  rawValue.reserve(stored.length());
  for (unsigned int i = 0; i < stored.length(); ++i)
  {
    rawValue.push_back(static_cast<char>(stored[i]));
  }

  RgbColor parsedColor;
  std::string normalized;
  if (!parseColorPayload(reinterpret_cast<const uint8_t *>(rawValue.data()), rawValue.size(), parsedColor, normalized))
  {
    setStrobeColorInternal(CRGB::White);
    return false;
  }

  setStrobeColorInternal(toCRGB(parsedColor));
  return true;
}

bool parseStrobeSpeedPayload(const uint8_t *data, size_t length, uint16_t &speedOut)
{
  if (data == nullptr || length == 0)
  {
    return false;
  }

  std::string ascii(reinterpret_cast<const char *>(data), length);
  ascii = trimCopy(ascii);
  if (!ascii.empty())
  {
    bool allDigits = true;
    for (char ch : ascii)
    {
      if (ch < '0' || ch > '9')
      {
        allDigits = false;
        break;
      }
    }

    if (allDigits)
    {
      unsigned long parsed = std::strtoul(ascii.c_str(), nullptr, 10);
      if (parsed > 0)
      {
        speedOut = static_cast<uint16_t>(parsed);
        return true;
      }
    }
  }

  if (length >= 2)
  {
    speedOut = static_cast<uint16_t>(
        static_cast<uint8_t>(data[0]) |
        (static_cast<uint16_t>(static_cast<uint8_t>(data[1])) << 8));
    return true;
  }

  speedOut = static_cast<uint8_t>(data[0]);
  return true;
}
} // namespace

void ensureStrobeSettingsLoaded()
{
  if (gStrobeSettings.loaded)
  {
    return;
  }

  loadStrobeColorFromPreferences();
  setStrobeCycleInternal(getStrobeSpeedPreference());
  gStrobeSettings.loaded = true;
}

bool applyStrobeColorBytes(const uint8_t *data, size_t length, bool persistPreference, std::string *normalizedOut)
{
  ensureStrobeSettingsLoaded();

  RgbColor parsedColor;
  std::string normalized;
  if (!parseColorPayload(data, length, parsedColor, normalized))
  {
    return false;
  }

  setStrobeColorInternal(toCRGB(parsedColor));
  if (persistPreference)
  {
    saveStrobeColorPreference(String(normalized.c_str()));
  }
  if (normalizedOut != nullptr)
  {
    *normalizedOut = normalized;
  }
  return true;
}

bool applyStrobeSpeedBytes(const uint8_t *data, size_t length, bool persistPreference, uint16_t *speedOut)
{
  ensureStrobeSettingsLoaded();

  uint16_t parsedSpeed = 0;
  if (!parseStrobeSpeedPayload(data, length, parsedSpeed))
  {
    return false;
  }

  parsedSpeed = clampStrobeCycleMs(parsedSpeed);
  setStrobeCycleInternal(parsedSpeed);
  if (persistPreference)
  {
    saveStrobeSpeedPreference(parsedSpeed);
  }
  if (speedOut != nullptr)
  {
    *speedOut = parsedSpeed;
  }
  return true;
}

String getStrobeColorHexString()
{
  ensureStrobeSettingsLoaded();
  const std::string hex = colorToHexString(toRgbColor(gStrobeSettings.color));
  return String(hex.c_str());
}

uint16_t getStrobeSpeedMs()
{
  ensureStrobeSettingsLoaded();
  return gStrobeSettings.cycleMs;
}

void strobeEffect()
{
  ensureStrobeSettingsLoaded();

  auto *display = getStrobeDisplay();
  if (!display)
  {
    return;
  }

  uint16_t flashWindowMs = gStrobeSettings.cycleMs / 4;
  if (flashWindowMs < kMinFlashMs)
  {
    flashWindowMs = kMinFlashMs;
  }
  if (flashWindowMs > gStrobeSettings.cycleMs)
  {
    flashWindowMs = gStrobeSettings.cycleMs;
  }

  const unsigned long phaseMs = millis() % gStrobeSettings.cycleMs;
  const bool flashOn = phaseMs < flashWindowMs;
  const uint16_t color = flashOn
                             ? display->color565(gStrobeSettings.color.r, gStrobeSettings.color.g, gStrobeSettings.color.b)
                             : 0;
  display->fillScreen(color);
}
