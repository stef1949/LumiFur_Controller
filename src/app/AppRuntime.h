#pragma once
#include <cstdint>

void requestDisplayRefresh();
void notifyBleTask();
void scheduleLastViewPersist(uint8_t view);
void noteButtonInteraction(unsigned long nowMs);
bool shakeSuppressedByRecentButton(unsigned long nowMs);
void resetBlePairing();
bool viewUsesMic(int view);
void updateGlobalBrightnessScale(uint8_t brightness);

extern bool facePlasmaDirty;
extern char txt[64];
