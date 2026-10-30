#pragma once
#include <Arduino.h>
#include <FastLED.h>
#include <cstddef>
#include <cstdint>
#include <string>

void ensureStaticColorLoaded();
CRGB getStaticColorCached();
String getStaticColorHexString();
bool applyStaticColorBytes(const uint8_t *data, size_t length, bool persistPreference, std::string *normalizedOut);
