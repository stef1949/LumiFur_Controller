#pragma once

#include <cmath>
#include <cstdint>

namespace brightness
{
// Reaches about 95% of a fixed target in three time constants.
constexpr float kAutoTransitionTimeMs = 800.0f;
constexpr uint32_t kMaxTransitionStepMs = 100;

inline float smoothAutoBrightness(float current, float target, uint32_t elapsedMs)
{
  // Resume gradually after sleep or a stalled loop instead of catching up in a jump.
  if (elapsedMs > kMaxTransitionStepMs)
  {
    elapsedMs = kMaxTransitionStepMs;
  }
  const float alpha = 1.0f - std::exp(-static_cast<float>(elapsedMs) / kAutoTransitionTimeMs);
  return current + alpha * (target - current);
}
} // namespace brightness
