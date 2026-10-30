#pragma once

#include <cstdint>

namespace display_timing
{
// Unsigned subtraction handles tick rollover. A missed deadline resets the
// schedule and yields for one tick instead of rendering a catch-up burst.
inline uint32_t nextFrameDelay(uint32_t now, uint32_t &lastWake, uint32_t period)
{
  if (period == 0)
    period = 1;
  if (static_cast<uint32_t>(now - lastWake) >= period)
  {
    lastWake = now;
    return 1;
  }
  lastWake += period;
  return static_cast<uint32_t>(lastWake - now);
}
}
