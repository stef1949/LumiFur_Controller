#pragma once
#include <cstdint>

namespace runtime
{
constexpr unsigned long kBaseFrameIntervalMs = 8;
constexpr unsigned long kDvdFrameIntervalMs = 40;
constexpr unsigned long kAnimationIntervalMs = 8;
constexpr unsigned long kBrightnessIntervalMs = 20;
constexpr unsigned long kLuxIntervalMs = 500;
constexpr unsigned long kSleepCheckIntervalMs = 60;
constexpr unsigned long kBleStatusIntervalMs = 25;
constexpr uint8_t kProximityTriggerDelta = 6;
constexpr uint8_t kProximityReleaseDelta = 2;
}
