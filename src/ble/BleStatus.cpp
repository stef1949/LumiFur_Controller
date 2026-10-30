#include "ble/BleStatus.h"
#include "ble/ble_state.h"
#include "app/AppRuntime.h"
#include "app/AppStateSync.h"
#include "app/InputController.h"
#include "app/PowerManagement.h"
#include "app/ViewState.h"
#include "app/RuntimeDiagnostics.h"
#include "config/RuntimeSettings.h"
#include "hardware/deviceConfig.h"

namespace {
TaskHandle_t bleNotifyTaskHandle = nullptr;
unsigned long bluetoothStatusChangeMillis = 0;
}
void fadeInAndOutLED(uint8_t r, uint8_t g, uint8_t b)
{
  static int brightness = 0;
  static int step = 5;
  static unsigned long lastUpdate = 0;
  const uint8_t delayTime = 15;

  if (millis() - lastUpdate < delayTime)
    return;
  lastUpdate = millis();

  brightness += step;
  if (brightness >= 128 || brightness <= 0)
    step *= -1;

  statusPixel.setPixelColor(0,
                            (r * brightness) / 255,
                            (g * brightness) / 255,
                            (b * brightness) / 255);
  statusPixel.show();
}

void fadeBlueLED()
{
  fadeInAndOutLED(0, 0, 100); // Blue color
}


void notifyBleTask()
{
  if (deviceConnected && bleNotifyTaskHandle)
  {
    xTaskNotifyGive(bleNotifyTaskHandle);
  }
}

void handleBLEStatusLED()
{
  const PairingSnapshot pairingSnapshot = getPairingSnapshot();
  static bool lastPairingActive = false;
  static bool lastPasskeyValid = false;
  static uint32_t lastPasskey = 0;
  bool pixelShown = false;
  if (deviceConnected != oldDeviceConnected)
  {
    bluetoothStatusChangeMillis = millis();
    requestDisplayRefresh();
    if (deviceConnected)
    {
      statusPixel.setPixelColor(0, 0, 50, 0); // Green when connected
    }
    else
    {
      statusPixel.setPixelColor(0, 0, 0, 0); // Off when disconnected
      refreshBleAdvertising();
    }
    // statusPixel.show();
    oldDeviceConnected = deviceConnected;
  }
  if (pairingSnapshot.pairing != lastPairingActive ||
      pairingSnapshot.passkeyValid != lastPasskeyValid ||
      pairingSnapshot.passkey != lastPasskey)
  {
    requestDisplayRefresh();
    lastPairingActive = pairingSnapshot.pairing;
    lastPasskeyValid = pairingSnapshot.passkeyValid;
    lastPasskey = pairingSnapshot.passkey;
  }
  // Byte 6. Off skips pairing and advert blinks.
if (disableBleIndicatorLight)
  {
    statusPixel.setPixelColor(0, 0, 0, 0);
    statusPixel.show();
    return;
  }
  if (pairingSnapshot.pairing)
  {
    fadeInAndOutLED(128, 0, 128); // Purple fade when pairing
    pixelShown = true;
  }
  else if (shouldBleAdvertise())
  {
    fadeInAndOutLED(0, 0, 100); // Blue fade when disconnected
    pixelShown = true;
  }

  if (!pixelShown)
  {
    statusPixel.show();
  }
}

void resetBlePairing()
{
  if (!pServer)
  {
    return;
  }

  startPairingMode(PAIRING_MODE_WINDOW_MS);
  const std::vector<uint16_t> peers = pServer->getPeerDevices();
  if (!peers.empty())
  {
    setPairingResetPending(true);
    for (uint16_t connHandle : peers)
    {
      pServer->disconnect(connHandle);
    }
    return;
  }

  const bool cleared = NimBLEDevice::deleteAllBonds();
  setPairingResetPending(false);
  refreshBleAdvertising();
#if defined(DEBUG_BLE)
  Serial.printf("BLE pairing reset: bonds cleared=%s\n", cleared ? "true" : "false");
#endif
}

void bleNotifyTask(void *param)
{
  for (;;)
  {
    // block here until someone calls xTaskNotifyGive()
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    PERF_SCOPE(PerfBucket::Ble);
#if DEBUG_MODE
    const uint32_t bleStartMicros = micros();
#endif
    if (deviceConnected)
    {
      uint8_t bleViewValue;
      { AppStateLock stateLock; bleViewValue = static_cast<uint8_t>(currentView); }
      pFaceCharacteristic->setValue(&bleViewValue, 1);
      pFaceCharacteristic->notify();
      pConfigCharacteristic->notify();
    }
#if DEBUG_MODE
    const uint32_t bleElapsed = micros() - bleStartMicros;
    if (bleElapsed > SLOW_SECTION_THRESHOLD_US)
    {
      Serial.printf("BLE notify slow: %lu us\n", static_cast<unsigned long>(bleElapsed));
    }
#endif
  }
}

bool startBleNotifications()
{
  return xTaskCreatePinnedToCore(bleNotifyTask, "BLE Task", 4096, nullptr, 1,
                               &bleNotifyTaskHandle, CONFIG_BT_NIMBLE_PINNED_TO_CORE) == pdPASS;
}
uint32_t bleNotificationStackHighWaterMark()
{
  return bleNotifyTaskHandle ? uxTaskGetStackHighWaterMark(bleNotifyTaskHandle) : 0;
}
unsigned long bleStatusChangedAt() { return bluetoothStatusChangeMillis; }
void ensureBleStatusTimestamp(unsigned long nowMs)
{
  if (!bluetoothStatusChangeMillis) bluetoothStatusChangeMillis = nowMs;
}
