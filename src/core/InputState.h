#pragma once
#include <cstdint>

namespace input
{
struct ButtonState
{
  bool rawPressed = false;
  bool stablePressed = false;
  bool longPressHandled = false;
  bool shortPressPending = false;
  uint32_t lastChangeMs = 0;
  uint32_t pressedAtMs = 0;
};

inline void updateButtonState(ButtonState &button, bool sample, uint32_t now, uint32_t debounce)
{
  if (sample != button.rawPressed) { button.rawPressed = sample; button.lastChangeMs = now; }
  if (now - button.lastChangeMs < debounce || button.stablePressed == sample) return;
  button.stablePressed = sample;
  if (sample)
  {
    button.pressedAtMs = now;
    button.longPressHandled = false;
    button.shortPressPending = false;
  }
  else if (!button.longPressHandled) button.shortPressPending = true;
}

inline void suppressButtonPress(ButtonState &button)
{
  button.shortPressPending = false;
  if (button.stablePressed) button.longPressHandled = true;
}

inline bool consumeButtonShortPress(ButtonState &button)
{
  const bool pending = button.shortPressPending;
  button.shortPressPending = false;
  return pending;
}

inline bool consumeButtonLongPress(ButtonState &button, uint32_t now, uint32_t hold)
{
  if (!button.stablePressed || button.longPressHandled || now - button.pressedAtMs < hold) return false;
  button.longPressHandled = true;
  button.shortPressPending = false;
  return true;
}
}
