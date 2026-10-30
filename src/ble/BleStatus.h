#pragma once
#include <cstdint>
void handleBLEStatusLED();
bool startBleNotifications();
uint32_t bleNotificationStackHighWaterMark();
unsigned long bleStatusChangedAt();
void ensureBleStatusTimestamp(unsigned long nowMs);
