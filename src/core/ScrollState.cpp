#include "core/ScrollState.h"

namespace scroll
{

namespace
{
State gState{};

constexpr std::uint8_t kMinSpeed = 1;
constexpr std::uint8_t kMaxSpeed = 100;
constexpr std::uint16_t kFastestInterval = 15;
constexpr std::uint16_t kSlowestInterval = 250;

std::uint8_t clampSpeed(std::uint8_t speed)
{
  if (speed < kMinSpeed)
  {
    return kMinSpeed;
  }
  if (speed > kMaxSpeed)
  {
    return kMaxSpeed;
  }
  return speed;
}

std::uint16_t computeInterval(std::uint8_t speed)
{
  const std::uint8_t clampedSpeed = clampSpeed(speed);
  const std::uint16_t range = kSlowestInterval - kFastestInterval;
  const std::uint16_t steps = static_cast<std::uint16_t>(kMaxSpeed - kMinSpeed);
  const std::uint16_t offset = static_cast<std::uint16_t>(
      ((static_cast<std::uint32_t>(clampedSpeed - kMinSpeed) * range) / (steps == 0 ? 1 : steps)));
  return static_cast<std::uint16_t>(kFastestInterval + offset);
}

struct Initializer
{
  Initializer()
  {
    gState.textIntervalMs = computeInterval(gState.speedSetting);
  }
};

Initializer initializer{};

} // namespace

State &state()
{
  return gState;
}

void updateIntervalFromSpeed()
{
  gState.textIntervalMs = computeInterval(gState.speedSetting);
}

void resetTiming()
{
  gState.lastScrollTickMs = 0;
  gState.lastBackgroundTickMs = 0;
  gState.backgroundOffset = 0;
  gState.colorOffset = 0;
  gState.textInitialized = false;
}

} // namespace scroll

