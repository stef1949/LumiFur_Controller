#pragma once

#include <cstdint>

namespace lux
{
// Below this, clear-only lux is often single digits indoors, so any change is sent.
constexpr uint16_t kSmallLuxThreshold = 20;

// Notify on any change while small, otherwise on a change of at least 10% of the last sent value.
inline bool shouldNotifyLux(uint16_t current, uint16_t lastSent)
{
  if (current == lastSent)
  {
    return false;
  }
  if (lastSent < kSmallLuxThreshold)
  {
    return true;
  }
  const int32_t delta = static_cast<int32_t>(current) - static_cast<int32_t>(lastSent);
  const int32_t magnitude = delta < 0 ? -delta : delta;
  int32_t threshold = lastSent / 10;
  if (threshold < 1)
  {
    threshold = 1;
  }
  return magnitude >= threshold;
}
} // namespace lux
