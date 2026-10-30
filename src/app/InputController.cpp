#include "app/InputController.h"
#include "core/AnimationState.h"
#include "app/RuntimeDiagnostics.h"
#include "app/Timing.h"
#include "app/RuntimeTuning.h"
#include "app/Sensors.h"
#include "config/RuntimeSettings.h"
#include "core/AdaptiveBrightness.h"
#include "effects/dinoGame.h"
#include "app/AppRuntime.h"
#include "app/ViewState.h"
#include "app/PowerManagement.h"
#include "core/InputState.h"
#include "ble/ble_state.h"
#include "hardware/deviceConfig.h"
#include <Arduino.h>

using namespace input;
namespace
{
constexpr unsigned long BUTTON_DEBOUNCE_MS = 50;
constexpr unsigned long PAIRING_MODE_HOLD_MS = 3000;
constexpr unsigned long PAIRING_RESET_HOLD_MS = 3000;
}

void handleButtonInputs(unsigned long loopNow)
{
// --- Handle button inputs for view changes and BLE pairing ---
#if defined(BUTTON_UP) || defined(BUTTON_DOWN)
#ifdef BUTTON_UP
    static ButtonState upButtonState;
#endif
#ifdef BUTTON_DOWN
    static ButtonState downButtonState;
#endif
#if defined(BUTTON_UP) && defined(BUTTON_DOWN)
    static bool pairingHoldActive = false;
    static bool pairingHoldTriggered = false;
    static unsigned long pairingHoldStartMs = 0;
#endif

#ifdef BUTTON_UP
    updateButtonState(upButtonState, digitalRead(BUTTON_UP) == LOW, loopNow, BUTTON_DEBOUNCE_MS);
#endif
#ifdef BUTTON_DOWN
    updateButtonState(downButtonState, digitalRead(BUTTON_DOWN) == LOW, loopNow, BUTTON_DEBOUNCE_MS);
#endif

#ifdef BUTTON_UP
    const bool upPressed = upButtonState.stablePressed;
#endif
#ifdef BUTTON_DOWN
    const bool downPressed = downButtonState.stablePressed;
#endif
    bool wakePressed = false;
#ifdef BUTTON_UP
    wakePressed = wakePressed || upPressed;
#endif
#ifdef BUTTON_DOWN
    wakePressed = wakePressed || downPressed;
#endif
    if (wakePressed)
    {
      wakeFromSleepMode();
      noteButtonInteraction(loopNow);
      lastActivityTime = loopNow;
    }
#if defined(BUTTON_UP) && defined(BUTTON_DOWN)
    const bool pairingResetHoldPressed = upPressed && downPressed;
#else
    const bool pairingResetHoldPressed = false;
#endif

#if defined(BUTTON_UP) && defined(BUTTON_DOWN)
    if (pairingResetHoldPressed)
    {
      suppressButtonPress(upButtonState);
      suppressButtonPress(downButtonState);
      noteButtonInteraction(loopNow);
      lastActivityTime = loopNow;
      if (!pairingHoldActive)
      {
        pairingHoldActive = true;
        pairingHoldStartMs = loopNow;
      }
      else if (!pairingHoldTriggered && (loopNow - pairingHoldStartMs >= PAIRING_RESET_HOLD_MS))
      {
        pairingHoldTriggered = true;
        resetBlePairing();
      }
    }
    else
    {
      pairingHoldActive = false;
      pairingHoldTriggered = false;
    }
#endif

    bool pairingModeTriggered = false;

    auto enablePairingModeFromHeldButton = [&]()
    {
      noteButtonInteraction(loopNow);
      startPairingMode(PAIRING_MODE_WINDOW_MS);
      refreshBleAdvertising();
      lastActivityTime = loopNow;
      pairingModeTriggered = true;
      requestDisplayRefresh();
    };

#ifdef BUTTON_UP
    if (!pairingResetHoldPressed && upPressed &&
        consumeButtonLongPress(upButtonState, loopNow, PAIRING_MODE_HOLD_MS))
    {
      enablePairingModeFromHeldButton();
    }
#endif

#ifdef BUTTON_DOWN
    if (!pairingResetHoldPressed && downPressed &&
        consumeButtonLongPress(downButtonState, loopNow, PAIRING_MODE_HOLD_MS))
    {
      enablePairingModeFromHeldButton();
    }
#endif

#ifdef BUTTON_UP
    if (!pairingResetHoldPressed && consumeButtonShortPress(upButtonState))
    {
      noteButtonInteraction(loopNow);
      selectUserView(static_cast<uint8_t>((currentView + 1) % totalViews));
      lastActivityTime = loopNow;
    }
#endif

#ifdef BUTTON_DOWN
    if (!pairingResetHoldPressed && consumeButtonShortPress(downButtonState))
    {
      noteButtonInteraction(loopNow);
      selectUserView(currentView == 0 ? static_cast<uint8_t>(totalViews - 1) : static_cast<uint8_t>(currentView - 1));
      lastActivityTime = loopNow;
    }
#endif

    if (pairingModeTriggered)
    {
      Serial.println("BLE pairing mode enabled by long button press.");
    }


#endif

}

namespace {
constexpr unsigned long PROX_LUX_READ_GUARD_MS = 20;
constexpr unsigned long MOTION_CHECK_INTERVAL_MS = MOTION_SAMPLE_INTERVAL_FAST;
static unsigned long lastMotionCheckTime = 0;
unsigned long lastSensorReadTime = 0;
const unsigned long sensorInterval = 250; // sensor read throttled to reduce I2C pressure
constexpr uint8_t PROX_TRIGGER_DELTA = runtime::kProximityTriggerDelta; // How far above baseline to trigger
constexpr uint8_t PROX_RELEASE_DELTA = runtime::kProximityReleaseDelta; // How close to baseline to release
constexpr unsigned long PROX_LATCH_TIMEOUT_MS = 1200;
constexpr float PROX_BASELINE_ALPHA = 0.05f; // Baseline smoothing factor
static bool proximityLatchedHigh = false;    // Prevents retriggering until sensor settles
static unsigned long proximityLatchedAt = 0;
static float proximityBaseline = 0.0f;
static bool proximityBaselineValid = false;

unsigned long gLastButtonInteractionMs = 0;
}

void noteButtonInteraction(unsigned long nowMs)
{
  gLastButtonInteractionMs = nowMs;
}

bool shakeSuppressedByRecentButton(unsigned long nowMs)
{
  return !hasElapsedSince(nowMs, gLastButtonInteractionMs, LF_SHAKE_SUPPRESS_AFTER_BUTTON_MS);
}

void handleSensorInputs(unsigned long loopNow)
{
  // --- Motion Detection (for shake effect to change view) ---
  runIfElapsed(loopNow, lastMotionCheckTime, MOTION_CHECK_INTERVAL_MS, [&]()
               {
                 PERF_SCOPE(PerfBucket::Sensors);
                 PROFILE_SECTION("MotionDetection");
                 bool buttonPhysicallyPressed = false;
                 #ifdef BUTTON_UP
                 buttonPhysicallyPressed = buttonPhysicallyPressed || (digitalRead(BUTTON_UP) == LOW);
                 #endif
                 #ifdef BUTTON_DOWN
                 buttonPhysicallyPressed = buttonPhysicallyPressed || (digitalRead(BUTTON_DOWN) == LOW);
                 #endif

                 if (!buttonPhysicallyPressed &&
                     !shakeSuppressedByRecentButton(loopNow) &&
                     accelerometerEnabled &&
                     g_accelerometer_initialized &&
                     currentView != VIEW_FLUID_EFFECT &&
                     currentView != VIEW_DINO_GAME &&
                     currentView != VIEW_RAINBOW_GRADIENT &&
                     currentView != VIEW_RAINBOW_LINEAR_BAND &&
                     !animation::state().isEyeBouncing &&
                     !proximityLatchedHigh)
                 {
                   useShakeSensitivity = true; // Use high threshold for shake detection
                   if (detectMotion())
                   { // detectMotion uses the current useShakeSensitivity
                     if (currentView != VIEW_SPIRAL_EYES)
                     {
                       animation::state().previousView = userSelectedView;
                       currentView = VIEW_SPIRAL_EYES;
                       animation::state().spiralStartMillis = loopNow;
                       requestDisplayRefresh();
                       LOG_DEBUG_LN("Shake detected! Switching to Spiral View.");
                       notifyBleTask();
                       lastActivityTime = loopNow;
                     }
                   }
                   useShakeSensitivity = false; // Switch back to low threshold for general sleep/wake checks
                 }
               });

  // --- Proximity Sensor Logic: Blush Trigger AND Eye Bounce Trigger ---
  // static unsigned long lastSensorReadTime = 0; // Already global
  const bool deferProximityRead = shouldDeferProximityRead(loopNow, PROX_LUX_READ_GUARD_MS);
  if (!deferProximityRead)
  {
    runIfElapsed(loopNow, lastSensorReadTime, sensorInterval, [&]()
                 {
                  PERF_SCOPE(PerfBucket::Sensors);
#if DEBUG_MODE
                 PROFILE_SECTION("ProximitySensor");
#endif
#if defined(APDS_AVAILABLE) // Ensure sensor is available
                 const unsigned long sensorNow = loopNow;
                 if (currentView == VIEW_RAINBOW_GRADIENT || currentView == VIEW_RAINBOW_LINEAR_BAND)
                 {
                   proximityLatchedHigh = false;
                   return;
                 }

                 uint8_t proximity = 0;
                 uint32_t proxDuration = 0;
                 if (!readAmbientProximity(sensorNow, proximity, &proxDuration))
                 {
#if DEBUG_PROXIMITY
                   LOG_PROX("Prox skip @%lu ms\n", sensorNow);
#endif
                   return;
                 }

#if DEBUG_MODE
                 if (proxDuration > SLOW_SECTION_THRESHOLD_US)
                 {
                   LOG_DEBUG("Slow proximity read: %lu us\n", static_cast<unsigned long>(proxDuration));
                 }
#endif
#if DEBUG_PROXIMITY
                 LOG_PROX("Prox=%u dt=%lu us\n", proximity, proxDuration);
#endif

                 bool bounceJustTriggered = false; // Flag to avoid double blush trigger

                 if (!proximityBaselineValid)
                 {
                   proximityBaseline = static_cast<float>(proximity);
                   proximityBaselineValid = true;
                 }

                 int baselineRounded = static_cast<int>(proximityBaseline + 0.5f);
                 uint8_t triggerThreshold = static_cast<uint8_t>(
                     constrain(baselineRounded + PROX_TRIGGER_DELTA, 0, 255));
                 uint8_t releaseThreshold = static_cast<uint8_t>(
                     constrain(baselineRounded + PROX_RELEASE_DELTA, 0, 255));

                 if (!proximityLatchedHigh && proximity <= releaseThreshold)
                 {
                   proximityBaseline += PROX_BASELINE_ALPHA * (static_cast<float>(proximity) - proximityBaseline);
                   baselineRounded = static_cast<int>(proximityBaseline + 0.5f);
                   triggerThreshold = static_cast<uint8_t>(
                       constrain(baselineRounded + PROX_TRIGGER_DELTA, 0, 255));
                   releaseThreshold = static_cast<uint8_t>(
                       constrain(baselineRounded + PROX_RELEASE_DELTA, 0, 255));
                 }

                 // Require the reading to rise above the trigger threshold and then fall below the release threshold before re-triggering
                 if (proximityLatchedHigh)
                 {
                   const bool timeout = (sensorNow - proximityLatchedAt) >= PROX_LATCH_TIMEOUT_MS;
                   if (proximity <= releaseThreshold || (timeout && proximity < triggerThreshold))
                   {
                     proximityLatchedHigh = false;
#if DEBUG_PROXIMITY
                     LOG_PROX("Prox released at %u (rel=%u base=%d)\n", proximity, releaseThreshold, baselineRounded);
#endif
                   }
                   else
                   {
#if DEBUG_PROXIMITY
                     LOG_PROX("Prox latched high (%u), skipping trigger\n", proximity);
#endif
                     return;
                   }
                 }

                 if (proximity < triggerThreshold)
                 {
                   return;
                 }

                 proximityLatchedHigh = true;
                 proximityLatchedAt = sensorNow;

                 if (currentView == VIEW_DINO_GAME)
                 {
                   queueDinoJump();
                   lastActivityTime = sensorNow;
                   return;
                 }

                 // Count this proximity hit. A threshold replaces the bounce.
                 if (boopMode > 0)
                 {
                   handleBoop(sensorNow);
                   if (boopRevertAt != 0)
                   {
                     lastActivityTime = sensorNow;
                     return;
                   }
                 }

                 // Eye Bounce Trigger - Switch to View 17 (Circle Eyes) (MODIFIED)
                 if (!animation::state().isEyeBouncing)
                 {
                   LOG_DEBUG_LN("Proximity! Starting eye bounce sequence & switching to Circle Eyes (View 17).");
#if DEBUG_PROXIMITY
                   LOG_PROX("Bounce trigger prox=%u view=%d\n", proximity, currentView);
#endif

                    // Store the user face and switch to Circle Eyes (view 17)
                   if (currentView != VIEW_CIRCLE_EYES)
                   {
                     animation::state().viewBeforeEyeBounce = userSelectedView;
                     currentView = VIEW_CIRCLE_EYES; // Switch to "Circle Eyes" view
                     requestDisplayRefresh();
                     // saveLastView(currentView); // Optional: save temporary view 17
                     notifyBleTask();
                   }

                   animation::state().isEyeBouncing = true;
                   animation::state().eyeBounceStartTime = sensorNow;
                   animation::state().eyeBounceCount = 0;
                   lastActivityTime = sensorNow;
                   bounceJustTriggered = true; // Mark that bounce (and view switch) happened

                   // Also trigger blush effect to happen ON view 17 (Circle Eyes)
                   if (animation::state().blushState == animation::BlushState::Inactive)
                   { // Only trigger if not already blushing
                     LOG_DEBUG_LN("Proximity! Also triggering blush effect on Circle Eyes.");
#if DEBUG_PROXIMITY
                     LOG_PROX("Blush-on-bounce prox=%u\n", proximity);
#endif
                     animation::state().blushState = animation::BlushState::FadeIn;
                     animation::state().blushStateStartTime = sensorNow;
                     animation::state().wasBlushOverlay = false;                       // Blush on view 17 is part of that view's temporary effect
                     animation::state().originalViewBeforeBlush = animation::state().viewBeforeEyeBounce; // Blush on view 17 uses context of animation::state().viewBeforeEyeBounce
                   }
                 }


                 if (!bounceJustTriggered)
                 {
                   if (currentView != VIEW_BLUSH_FACE && currentView != VIEW_CIRCLE_EYES)
                   {
                     LOG_DEBUG_LN("Proximity! Triggering boop.");
                     handleBoop(sensorNow);
                     lastActivityTime = sensorNow;
                   }
                 }
#endif
                 });
  }

}
