#pragma once
#include <cstdint>

namespace power
{
enum class Action { None, Wake, Sleep };

inline Action evaluate(bool sleeping, bool enabled, bool activity, bool otaActive,
                       uint32_t nowMs, uint32_t lastActivityMs, uint32_t timeoutMs)
{
  if (sleeping) return (!enabled || activity || otaActive) ? Action::Wake : Action::None;
  if (enabled && !activity && !otaActive && nowMs - lastActivityMs > timeoutMs) return Action::Sleep;
  return Action::None;
}
}
