#include "app/ViewState.h"
#include "app/Timing.h"
#include "app/RuntimeDiagnostics.h"
#include "app/AppRuntime.h"
#include "app/AppStateSync.h"
#include "app/PowerManagement.h"
#include "config/RuntimeSettings.h"
#include "core/AnimationState.h"
#include <Arduino.h>

uint8_t currentView = VIEW_FLAME_EFFECT;
uint8_t userSelectedView = VIEW_FLAME_EFFECT;
unsigned long boopRevertAt = 0;
bool boopLoadingActive = false;
namespace
{
unsigned long boopHits[16] = {};
uint8_t boopHitCount = 0;
uint8_t boopSavedView = 0;
}

void selectUserView(uint8_t view)
{
  AppStateLock stateLock;
  if (view >= totalViews) return;
  wakeFromSleepMode();
  boopRevertAt = 0;
  boopLoadingActive = false;
  auto &state = animation::state();
  state.isEyeBouncing = false;
  state.currentEyeYOffset = 0;
  state.eyeBounceCount = 0;
  state.blushState = animation::BlushState::Inactive;
  state.blushBrightness = 0;
  state.spiralStartMillis = 0;
  userSelectedView = view;
  currentView = view;
  scheduleLastViewPersist(view);
  lastActivityTime = millis();
  requestDisplayRefresh();
  notifyBleTask();
}

void updateTemporaryView(unsigned long now)
{
  if (boopRevertAt && static_cast<int32_t>(static_cast<uint32_t>(now - boopRevertAt)) >= 0)
  {
    boopRevertAt = 0;
    boopLoadingActive = false;
    currentView = boopSavedView;
    notifyBleTask();
    requestDisplayRefresh();
  }
}

void handleBoop(unsigned long now)
{
  uint8_t kept = 0;
  for (uint8_t i = 0; i < boopHitCount; ++i)
  {
    if (now - boopHits[i] <= 15000UL)
      boopHits[kept++] = boopHits[i];
  }
  boopHitCount = kept;
  if (boopHitCount == 16)
  {
    for (uint8_t i = 1; i < boopHitCount; ++i)
      boopHits[i - 1] = boopHits[i];
    --boopHitCount;
  }
  boopHits[boopHitCount++] = now;

  uint8_t hits10s = 0;
  const uint8_t hits15s = boopHitCount;
  for (uint8_t i = 0; i < boopHitCount; ++i)
  {
    if (now - boopHits[i] <= 10000UL)
      ++hits10s;
  }
  Serial.printf("Boop mode=%u hits10=%u hits15=%u\n", boopMode, hits10s, hits15s);
  const unsigned long blushSpan = fadeInDuration + fullDuration + fadeOutDuration;

  int specialView = -1;
  if (boopMode >= 3 && hits15s >= 15)
  {
    specialView = VIEW_LOADING_BAR;
  }
  else if (boopMode >= 2 && hits15s >= 10)
  {
    specialView = VIEW_ANGRY_FACE;
  }
  else if (boopMode >= 1 && hits10s >= 5)
  {
    specialView = VIEW_SPIRAL_EYES;
  }

  if (specialView >= 0)
  {
    animation::state().blushState = animation::BlushState::Inactive;
    animation::state().wasBlushOverlay = false;
    boopSavedView = userSelectedView;
    currentView = static_cast<uint8_t>(specialView);
    boopRevertAt = now + (specialView == VIEW_LOADING_BAR ? 3000UL : blushSpan);
    notifyBleTask();
    requestDisplayRefresh();
    return;
  }

  if (animation::state().blushState == animation::BlushState::Inactive)
  {
    animation::state().blushState = animation::BlushState::FadeIn;
    animation::state().blushStateStartTime = now;
    animation::state().originalViewBeforeBlush = userSelectedView;
    animation::state().wasBlushOverlay = true;
  }
}

void updateViewTimers(unsigned long loopNow)
{
  // --- Revert from Spiral View Timer ---
  // Use a local copy to avoid updating animation::state().spiralStartMillis inside hasElapsedSince
  unsigned long spiralStartMillisCopy = animation::state().spiralStartMillis;
  if (currentView == VIEW_SPIRAL_EYES && spiralStartMillisCopy > 0 && hasElapsedSince(loopNow, spiralStartMillisCopy, 5000))
  {
    LOG_DEBUG_LN("Spiral timeout, reverting view.");
    currentView = animation::state().previousView;
    animation::state().spiralStartMillis = 0;
    requestDisplayRefresh();
    notifyBleTask();
  }

  updateTemporaryView(loopNow);

}
