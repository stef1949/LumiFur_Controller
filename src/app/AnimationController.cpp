#include "app/AnimationController.h"
#include "app/AppRuntime.h"
#include "app/Timing.h"
#include "app/RuntimeTuning.h"
#include "app/ViewState.h"
#include "app/RuntimeDiagnostics.h"
#include "rendering/RenderState.h"
#include "core/Easing.h"
#include <FastLED.h>
#include <cmath>

using animation::AnimationState;
using animation::BlinkTimings;
using animation::BlushState;

AnimationState &gAnimationState = animation::state();

BlinkTimings &blinkState = gAnimationState.blinkTimings;
unsigned long &lastEyeBlinkTime = gAnimationState.lastEyeBlinkTime;
unsigned long &nextBlinkDelay = gAnimationState.nextBlinkDelay;
int &blinkProgress = gAnimationState.blinkProgress;
bool &isBlinking = gAnimationState.isBlinking;
bool &manualBlinkTrigger = gAnimationState.manualBlinkTrigger;
bool &isEyeBouncing = gAnimationState.isEyeBouncing;
unsigned long &eyeBounceStartTime = gAnimationState.eyeBounceStartTime;
volatile int &currentEyeYOffset = gAnimationState.currentEyeYOffset;
int &eyeBounceCount = gAnimationState.eyeBounceCount;
uint8_t &viewBeforeEyeBounce = gAnimationState.viewBeforeEyeBounce;
volatile int &idleEyeYOffset = gAnimationState.idleEyeYOffset;
volatile int &idleEyeXOffset = gAnimationState.idleEyeXOffset;
unsigned long &spiralStartMillis = gAnimationState.spiralStartMillis;
int &previousView = gAnimationState.previousView;
volatile BlushState &blushState = gAnimationState.blushState;
volatile unsigned long &blushStateStartTime = gAnimationState.blushStateStartTime;
bool &isBlushFadingIn = gAnimationState.isBlushFadingIn;
uint8_t &originalViewBeforeBlush = gAnimationState.originalViewBeforeBlush;
bool &wasBlushOverlay = gAnimationState.wasBlushOverlay;
volatile uint8_t &blushBrightness = gAnimationState.blushBrightness;

inline void setBlinkProgress(int newValue)
{
  if (blinkProgress != newValue)
  {
    facePlasmaDirty = true;
  }
  blinkProgress = newValue;
}

constexpr int MIN_BLINK_DURATION = 300;
constexpr int MAX_BLINK_DURATION = 800;
constexpr int MIN_BLINK_DELAY = 250;
constexpr int MAX_BLINK_DELAY = 5000;

constexpr unsigned long EYE_BOUNCE_DURATION = 400;
constexpr int EYE_BOUNCE_AMPLITUDE = 5;
constexpr int MAX_EYE_BOUNCES = 10;

const int IDLE_HOVER_AMPLITUDE_Y = 2.5;
const int IDLE_HOVER_AMPLITUDE_X = 2.5;
const unsigned long IDLE_HOVER_PERIOD_MS_Y = 3000;
const unsigned long IDLE_HOVER_PERIOD_MS_X = 4200;

const int minBlinkDuration = MIN_BLINK_DURATION;
const int maxBlinkDuration = MAX_BLINK_DURATION;
const int minBlinkDelay = MIN_BLINK_DELAY;
const int maxBlinkDelay = MAX_BLINK_DELAY;

void initBlinkState()
{
  blinkState.startTime = 0;
  blinkState.durationMs = 700;
  blinkState.closeDuration = 50;
  blinkState.holdDuration = 20;
  blinkState.openDuration = 50;
}

void updateIdleHoverAnimation()
{
  const uint32_t currentMs = millis();

  const int prevIdleYOffset = idleEyeYOffset;
  const int prevIdleXOffset = idleEyeXOffset;

  if (IDLE_HOVER_PERIOD_MS_Y > 0)
  {
    const uint32_t phaseY = (static_cast<uint64_t>(currentMs % IDLE_HOVER_PERIOD_MS_Y) << 16) / IDLE_HOVER_PERIOD_MS_Y;
    const int32_t sinY = sin16(static_cast<uint16_t>(phaseY));
    idleEyeYOffset = static_cast<int>((sinY * IDLE_HOVER_AMPLITUDE_Y + 16384) >> 15);
  }
  else
  {
    idleEyeYOffset = 0;
  }

  if (IDLE_HOVER_PERIOD_MS_X > 0)
  {
    const uint32_t phaseX = (static_cast<uint64_t>(currentMs % IDLE_HOVER_PERIOD_MS_X) << 16) / IDLE_HOVER_PERIOD_MS_X;
    const int32_t cosX = cos16(static_cast<uint16_t>(phaseX));
    idleEyeXOffset = static_cast<int>((cosX * IDLE_HOVER_AMPLITUDE_X + 16384) >> 15);
  }
  else
  {
    idleEyeXOffset = 0;
  }

  if (idleEyeYOffset != prevIdleYOffset || idleEyeXOffset != prevIdleXOffset)
  {
    facePlasmaDirty = true;
  }
}

void updateEyeBounceAnimation()
{
  int newOffset = currentEyeYOffset;

  if (!isEyeBouncing)
  {
    eyeBounceCount = 0;
    if (newOffset != 0)
    {
      newOffset = 0;
      facePlasmaDirty = true;
    }
    currentEyeYOffset = newOffset;
    return;
  }

  unsigned long elapsed = millis() - eyeBounceStartTime;

  if (elapsed >= EYE_BOUNCE_DURATION)
  {
    eyeBounceCount++;
    if (eyeBounceCount >= MAX_EYE_BOUNCES)
    {
      isEyeBouncing = false;
      newOffset = 0;
      eyeBounceCount = 0;

      if (currentView == VIEW_CIRCLE_EYES)
      {
        currentView = viewBeforeEyeBounce;
        scheduleLastViewPersist(currentView);
        requestDisplayRefresh();
        LOG_DEBUG("Eye bounce finished. Reverting to view: %d\n", currentView);
        notifyBleTask();
      }

      if (currentEyeYOffset != newOffset)
      {
        facePlasmaDirty = true;
      }
      currentEyeYOffset = newOffset;
      return;
    }
    else
    {
      eyeBounceStartTime = millis();
      elapsed = 0;
    }
  }

  const unsigned long halfDuration = EYE_BOUNCE_DURATION / 2;
  if (elapsed < halfDuration)
  {
    newOffset = map(elapsed, 0, halfDuration, 0, EYE_BOUNCE_AMPLITUDE);
  }
  else
  {
    newOffset = map(elapsed, halfDuration, EYE_BOUNCE_DURATION, EYE_BOUNCE_AMPLITUDE, 0);
  }

  if (newOffset != currentEyeYOffset)
  {
    currentEyeYOffset = newOffset;
    facePlasmaDirty = true;
  }
}

void updateBlinkAnimation()
{
  const unsigned long now = millis();

  // Check if it's time to start a new blink
  if (manualBlinkTrigger || (!isBlinking && (now - lastEyeBlinkTime >= nextBlinkDelay)))
  {
    isBlinking = true;
    manualBlinkTrigger = false;
    blinkState.startTime = now;

    // Randomize total blink duration and phase ratios, like in the emulator
    unsigned long totalDuration = 400 + random(301); // 400 to 700 ms
    float closeRatio = 0.2f + (random(21) / 100.0f); // 0.2 to 0.4
    float holdRatio = 0.05f + (random(11) / 100.0f); // 0.05 to 0.15

    blinkState.closeDuration = totalDuration * closeRatio;
    blinkState.holdDuration = totalDuration * holdRatio;
    blinkState.openDuration = totalDuration * (1.0f - closeRatio - holdRatio);
    blinkProgress = 0;
  }

  // If a blink is in progress, update its state
  if (isBlinking)
  {
    const unsigned long elapsed = now - blinkState.startTime;
    const unsigned long totalDuration = blinkState.closeDuration + blinkState.holdDuration + blinkState.openDuration;

    if (elapsed < blinkState.closeDuration)
    {
      float t = (float)elapsed / blinkState.closeDuration;
      blinkProgress = 100 * easeInQuad(t);
    }
    else if (elapsed < blinkState.closeDuration + blinkState.holdDuration)
    {
      blinkProgress = 100;
    }
    else if (elapsed < totalDuration)
    {
      float t = (float)(elapsed - blinkState.closeDuration - blinkState.holdDuration) / blinkState.openDuration;
      blinkProgress = 100 * (1.0f - easeOutQuad(t));
    }
    else
    {
      // Blink finished
      isBlinking = false;
      blinkProgress = 0;
      lastEyeBlinkTime = now;
      // Calculate delay for the next blink
      nextBlinkDelay = minBlinkDelay + random(maxBlinkDelay - minBlinkDelay + 1);
      // 15% chance of a quick "double blink"
      if (random(100) < 15)
      {
        nextBlinkDelay = 100 + random(151);
      }
    }
  }
}

void disableBlush()
{
  Serial.println("Blush disabled!");
  facePlasmaDirty = true; // Defer actual redraw to display task
}

void updateBlush()
{
  unsigned long now = millis();
  unsigned long elapsed = now - blushStateStartTime;

  switch (blushState)
  {
  case BlushState::FadeIn:
    if (elapsed < fadeInDuration)
    {
      // Use ease-in for a smooth start
      float progress = (float)elapsed / fadeInDuration;
      blushBrightness = 255 * easeInQuad(progress);
    }
    else
    {
      blushBrightness = 255;
      blushState = BlushState::Full;
      blushStateStartTime = now; // Restart timer for full brightness phase
    }
    break;

  case BlushState::Full:
    if (elapsed >= fullDuration)
    {
      // After full brightness time, start fading out
      blushState = BlushState::FadeOut;
      blushStateStartTime = now; // Restart timer for fade-out
    }
    // Brightness remains at 255 during this phase.
    break;

  case BlushState::FadeOut:
    if (elapsed < fadeOutDuration)
    {
      // Use ease-out for a smooth end
      float progress = (float)elapsed / fadeOutDuration;
      blushBrightness = 255 * (1.0f - easeOutQuad(progress));
    }
    else
    {
      blushBrightness = 0;
      blushState = BlushState::Inactive;
      disableBlush();

      // Revert to previous view if this was an overlay blush
      if (wasBlushOverlay)
      {
        currentView = originalViewBeforeBlush;
        scheduleLastViewPersist(currentView);
        requestDisplayRefresh();
        Serial.printf("Blush overlay finished. Reverting to view: %d\n", currentView);
        notifyBleTask();
        wasBlushOverlay = false; // Reset the flag
      }
    }
    break;

  default:
    wasBlushOverlay = false; // Ensure flag is reset if state becomes inactive
    break;
  }
}

void updateViewAnimations(unsigned long nowMs)
{
  static unsigned long lastUpdateMs = 0;
  runIfElapsed(nowMs, lastUpdateMs, runtime::kAnimationIntervalMs, []() {
    PERF_SCOPE(PerfBucket::Animation);
    PROFILE_SECTION("AnimationUpdates");
    updateBlinkAnimation();
    updateEyeBounceAnimation();
    updateIdleHoverAnimation();
  });
  if (blushState != BlushState::Inactive)
  {
    PROFILE_SECTION("BlushUpdate");
    updateBlush();
  }
}
