#include <Arduino.h>
#include "app/AppSetup.h"
#include "app/AppStateSync.h"
#include "app/AnimationController.h"
#include "app/InputController.h"
#include "app/PowerManagement.h"
#include "app/RuntimeDiagnostics.h"
#include "app/Timing.h"
#include "app/RuntimeTuning.h"
#include "app/ViewState.h"
#include "app/AppRuntime.h"
#include "ble/BleStatus.h"
#include "ble/ble_state.h"
#include "ble/ble_metrics.h"
#include "config/ViewPersistence.h"
#include "core/AdaptiveBrightness.h"

void setup()
{
  initializeApplication();
}

void loop()
{
  AppStateLock stateLock;
  const uint32_t started = micros();
  const unsigned long now = millis();
  static unsigned long lastBrightness = 0, lastLux = 0, lastSleep = 0, lastBleStatus = 0;

  if (expirePairingModeIfNeeded())
  {
    refreshBleAdvertising();
    requestDisplayRefresh();
  }
  flushPendingLastViewPersist(now);
  handleButtonInputs(now);

  if (!sleepModeActive)
  {
    updateViewAnimations(now);
    handleSensorInputs(now);
    updateViewTimers(now);
    runIfElapsed(now, lastBrightness, runtime::kBrightnessIntervalMs, []() {
      PERF_SCOPE(PerfBucket::Sensors);
      PROFILE_SECTION("AutoBrightness");
      maybeUpdateBrightness();
    });
    if (deviceConnected)
      runIfElapsed(now, lastLux, runtime::kLuxIntervalMs, []() {
        PERF_SCOPE(PerfBucket::Ble);
        updateLux();
      });
  }

  updateTemperature(); // Collection continues while disconnected or asleep.
  runIfElapsed(now, lastSleep, runtime::kSleepCheckIntervalMs, []() {
    PERF_SCOPE(PerfBucket::Sensors);
    checkSleepMode();
  });
  runIfElapsed(now, lastBleStatus, runtime::kBleStatusIntervalMs, []() {
    PERF_SCOPE(PerfBucket::Io);
    handleBLEStatusLED();
  });
  perfRecordLoopDuration(micros() - started);
  perfMaybeReport(now);
  const TickType_t idleTicks = sleepModeActive ? pdMS_TO_TICKS(5) : pdMS_TO_TICKS(1);
  stateLock.unlock();
  vTaskDelay(idleTicks);
}
