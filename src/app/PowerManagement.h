#pragma once
#include <cstdint>

extern bool sleepModeActive;
extern unsigned long lastActivityTime;
extern unsigned long sleepFrameInterval;
extern unsigned long SLEEP_TIMEOUT_MS;
constexpr unsigned long SLEEP_TIMEOUT_MS_DEBUG = 600000;
constexpr unsigned long SLEEP_TIMEOUT_MS_NORMAL = 600000;
constexpr uint8_t sleepBrightness = 15;

void wakeFromSleepMode();
void enterSleepMode();
void checkSleepMode();
