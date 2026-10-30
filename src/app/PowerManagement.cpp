#include "app/PowerManagement.h"
#include "app/AppRuntime.h"
#include "app/Sensors.h"
#include "app/ViewState.h"
#include "config/RuntimeSettings.h"
#include "config/debug_config.h"
#include "core/PowerPolicy.h"
#include "core/AdaptiveBrightness.h"
#include "core/mic/mic.h"
#include "ble/ble_state.h"
#include "ble/ble_worker.h"
#include <Arduino.h>

bool sleepModeActive = false;
unsigned long lastActivityTime = 0;
unsigned long sleepFrameInterval = 50;
unsigned long SLEEP_TIMEOUT_MS = SLEEP_TIMEOUT_MS_NORMAL;
namespace { uint8_t preSleepView = VIEW_NORMAL_FACE; }

void wakeFromSleepMode()
{
  if (!sleepModeActive)
    return; // Already awake

  Serial.println(">>> Waking from sleep mode <<<");
  sleepModeActive = false;
  currentView = preSleepView; // Restore previous view
  micSetEnabled(viewUsesMic(currentView));
  requestDisplayRefresh();

  // Restore normal CPU speed
  setCpuFrequencyMhz(240);

  // Restore normal frame rate
  sleepFrameInterval = 5; // Back to ~90 FPS

  // Restore normal brightness
  updateGlobalBrightnessScale(userBrightness);
  syncBrightnessState(userBrightness);

  lastActivityTime = millis(); // Reset activity timer

  // Notify all clients if connected
  if (deviceConnected)
  {
    // Also send current view back to the app
    uint8_t bleViewValue = static_cast<uint8_t>(currentView);
    pFaceCharacteristic->setValue(&bleViewValue, 1);
    pFaceCharacteristic->notify();


  }

  // Restore normal BLE advertising if not connected
  if (!deviceConnected)
  {
    NimBLEDevice::getAdvertising()->setMinInterval(160); // 100 ms (default)
    NimBLEDevice::getAdvertising()->setMaxInterval(240); // 150 ms (default)
    refreshBleAdvertising();
  }
}

void enterSleepMode()
{
  if (sleepModeActive)
    return; // Already sleeping
  #if DEBUG_MODE
  Serial.println("Entering sleep mode");
  #endif
  sleepModeActive = true;
  micSetEnabled(false);
  preSleepView = currentView;                   // Save current view
  updateGlobalBrightnessScale(sleepBrightness);
  requestDisplayRefresh();

  setCpuFrequencyMhz(80); // Reduce CPU speed for power saving

  sleepFrameInterval = 100; // 10 FPS during sleep to save power

  /// Reduce BLE activity
  if (!deviceConnected)
  {
    NimBLEDevice::getAdvertising()->stop();               // Stop normal advertising
    NimBLEDevice::getAdvertising()->setMinInterval(2400); // 1500 ms
    NimBLEDevice::getAdvertising()->setMaxInterval(4800); // 3000 ms
    refreshBleAdvertising();
    #if DEBUG_MODE
    Serial.println("Reduced BLE Adv interval for sleep.");
    #endif
  }
  // No need to change sleepFrameInterval here, main loop timing controls frame rate
}

void checkSleepMode()
{
  const uint32_t now = millis();
  const bool activity = sleepMotionDetected();
  if (activity || bleIsOtaBusy()) lastActivityTime = now;
  switch (power::evaluate(sleepModeActive, sleepModeEnabled, activity, bleIsOtaBusy(),
                          now, lastActivityTime, SLEEP_TIMEOUT_MS))
  {
  case power::Action::Wake: wakeFromSleepMode(); break;
  case power::Action::Sleep: enterSleepMode(); break;
  default: break;
  }
}
