#ifndef STROBE_EFFECT_H
#define STROBE_EFFECT_H

#include <Arduino.h>
#include <cstddef>
#include <cstdint>
#include <string>

#include <FastLED.h>

void strobeEffect();
void ensureStrobeSettingsLoaded();
bool applyStrobeColorBytes(const uint8_t *data, size_t length, bool persistPreference, std::string *normalizedOut);
bool applyStrobeSpeedBytes(const uint8_t *data, size_t length, bool persistPreference, uint16_t *speedOut);
String getStrobeColorHexString();
uint16_t getStrobeSpeedMs();

#endif // STROBE_EFFECT_H
