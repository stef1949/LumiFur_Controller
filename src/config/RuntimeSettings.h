#pragma once
#include <FastLED.h>
#include <cstdint>

extern bool autoBrightnessEnabled;
extern bool accelerometerEnabled;
extern bool sleepModeEnabled;
extern bool auroraModeEnabled;
extern bool staticColorModeEnabled;
extern bool mouthMicBrightnessOverrideEnabled;
extern bool disableBleIndicatorLight;
extern bool disableBleStatusIcon;
extern bool waveshareBrightnessBoost;
extern bool constantColorConfig;
extern CRGB constantColor;
void ensureStaticColorLoaded();
CRGB getStaticColorCached();
extern bool configApplyAutoBrightness;
extern bool configApplySleepMode;
extern bool configApplyAuroraMode;
extern bool configApplyAccelerometer;
extern bool configApplyConstantColor;
extern bool matrixRainInsteadOfPlasma;
extern uint8_t autoBrightnessFloor;
extern uint8_t boopMode;
extern uint8_t userBrightness;
void applyConfigOptions();
