#include "config/StaticColor.h"
#include "config/RuntimeSettings.h"
#include "config/userPreferences.h"
#include "core/ColorParser.h"
#include "app/AppRuntime.h"

struct StaticColorState
{
  CRGB color = CRGB::Black;
  bool hasValue = false;
};

static StaticColorState gStaticColorState;

static CRGB toCRGB(const RgbColor &color)
{
  return CRGB(color.r, color.g, color.b);
}

static RgbColor toRgbColor(const CRGB &color)
{
  RgbColor result;
  result.r = color.r;
  result.g = color.g;
  result.b = color.b;
  return result;
}

static bool parseColorPayloadToCRGB(const uint8_t *data, size_t length, CRGB &colorOut, std::string &normalizedHex)
{
  RgbColor parsed;
  if (!parseColorPayload(data, length, parsed, normalizedHex))
  {
    return false;
  }
  colorOut = toCRGB(parsed);
  return true;
}

static void setStaticColor(const CRGB &color)
{
  gStaticColorState.color = color;
  gStaticColorState.hasValue = true;
  constantColor = color;
  requestDisplayRefresh();
}

static void setStaticColorToDefault()
{
  setStaticColor(constantColor);
}

bool applyStaticColorBytes(const uint8_t *data, size_t length, bool persistPreference, std::string *normalizedOut)
{
  std::string normalized;
  CRGB parsedColor;
  if (!parseColorPayloadToCRGB(data, length, parsedColor, normalized))
  {
    return false;
  }

  setStaticColor(parsedColor);
  if (persistPreference)
  {
    saveSelectedColor(String(normalized.c_str()));
  }
  if (normalizedOut)
  {
    *normalizedOut = normalized;
  }
  return true;
}

static bool loadStaticColorFromPreferences()
{
  String stored = getSelectedColor();
  if (stored.length() == 0)
  {
    setStaticColorToDefault();
    return false;
  }

  std::string rawValue;
  rawValue.reserve(stored.length());
  for (unsigned int i = 0; i < stored.length(); ++i)
  {
    rawValue.push_back(static_cast<char>(stored[i]));
  }

  if (!applyStaticColorBytes(reinterpret_cast<const uint8_t *>(rawValue.data()), rawValue.size(), false, nullptr))
  {
    setStaticColorToDefault();
    return false;
  }
  return true;
}

void ensureStaticColorLoaded()
{
  if (!gStaticColorState.hasValue)
  {
    loadStaticColorFromPreferences();
  }
}

CRGB getStaticColorCached()
{
  ensureStaticColorLoaded();
  return gStaticColorState.color;
}

String getStaticColorHexString()
{
  ensureStaticColorLoaded();
  const std::string hex = colorToHexString(toRgbColor(gStaticColorState.color));
  return String(hex.c_str());
}

