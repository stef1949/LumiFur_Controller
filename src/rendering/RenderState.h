#pragma once
#include <FastLED.h>
#include <cstdint>

// Guarded by AppStateLock; the display task alone publishes DMA frames.
extern float globalBrightnessScale;
extern uint16_t globalBrightnessScaleFixed;
extern bool facePlasmaDirty;
extern CRGBPalette16 currentPalette;
extern uint16_t time_counter;
extern uint16_t plasmaFrameDelay;
extern unsigned long rotationFrameInterval;
extern int loadingProgress;
extern int loadingMax;
void updateGlobalBrightnessScale(uint8_t brightness);
void applyPendingDisplayBrightness();
void presentDisplayFrame();
uint16_t getNormalFaceSoftwareBrightnessScale();
uint8_t scaleRawFaceComponentForPanelHeadroom(uint8_t value);
void applyMouthMicBrightnessPolicy(bool allowForCurrentView);
bool mouthMicBrightnessOverrideActive();
uint8_t requestedPanelBrightness();
