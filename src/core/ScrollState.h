#pragma once

#include <cstdint>

namespace scroll
{

struct State
{
  std::uint16_t speedSetting = 4;      // 1-500
  std::uint16_t textIntervalMs = 15;  // milliseconds per pixel step
  std::uint32_t lastScrollTickMs = 0; // Timestamp of last scroll update
  std::uint32_t lastBackgroundTickMs = 0; // Timestamp of last background update
  std::uint8_t backgroundOffset = 0;
  std::uint8_t colorOffset = 0;
  bool textInitialized = false;
};

State &state();

void updateIntervalFromSpeed();
void resetTiming();

} // namespace scroll

