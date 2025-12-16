#pragma once

#include <cstdint>

namespace animation
{

enum class BlushState : std::uint8_t
{
  Inactive,
  FadeIn,
  Full,
  FadeOut
};

struct BlinkTimings
{
  unsigned long startTime = 0;
  unsigned long durationMs = 700;
  unsigned long closeDuration = 50;
  unsigned long holdDuration = 20;
  unsigned long openDuration = 50;
};

struct AnimationState
{
  // Blink
  unsigned long lastEyeBlinkTime = 0;
  unsigned long nextBlinkDelay = 1000;
  int blinkProgress = 0;
  bool isBlinking = false;
  bool manualBlinkTrigger = false;
  BlinkTimings blinkTimings{};

  // Eye bounce
  bool isEyeBouncing = false;
  unsigned long eyeBounceStartTime = 0;
  int currentEyeYOffset = 0;
  int eyeBounceCount = 0;
  std::uint8_t viewBeforeEyeBounce = 0;

  // Idle hover
  int idleEyeYOffset = 0;
  int idleEyeXOffset = 0;

  // Spiral
  unsigned long spiralStartMillis = 0;
  int previousView = 0;

  // Maw
  int currentMaw = 1;
  unsigned long mawChangeTime = 0;
  bool mawTemporaryChange = false;
  bool mouthOpen = false;
  unsigned long lastMouthTriggerTime = 0;

  // Blush
  BlushState blushState = BlushState::Inactive;
  unsigned long blushStateStartTime = 0;
  bool isBlushFadingIn = false;
  std::uint8_t originalViewBeforeBlush = 0;
  bool wasBlushOverlay = false;
  std::uint8_t blushBrightness = 0;
};

AnimationState &state();
void reset(AnimationState &state);

} // namespace animation

