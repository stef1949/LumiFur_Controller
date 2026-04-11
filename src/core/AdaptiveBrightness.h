#pragma once

#include <cstdint>

void setupAdaptiveBrightness();
uint16_t getRawClearChannelValue();
float getAmbientLux();
uint16_t getAmbientLuxU16();
void maybeUpdateBrightness();
bool shouldReadProximity(unsigned long now);
bool shouldDeferProximityRead(unsigned long now, unsigned long guardMs);
void syncBrightnessState(uint8_t brightness);
int getLastAppliedBrightness();
