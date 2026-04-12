#include "ble/ble_state.h"
#include "ble/ble_constants.h"

#include <Arduino.h>

#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"

bool deviceConnected = false;
bool oldDeviceConnected = false;
bool devicePairing = false;
uint32_t pairingPasskey = 0;
bool pairingPasskeyValid = false;
static bool pairingResetPending = false;
static bool pairingModeActive = false;
static unsigned long pairingModeDeadlineMs = 0;

static portMUX_TYPE pairingMux = portMUX_INITIALIZER_UNLOCKED;

namespace
{
bool hasDeadlineElapsed(unsigned long nowMs, unsigned long deadlineMs)
{
  return static_cast<long>(nowMs - deadlineMs) >= 0;
}

void clearPairingStateLocked(bool keepPairingMode, bool clearPasskey)
{
  devicePairing = keepPairingMode;
  pairingPasskeyValid = false;
  if (clearPasskey)
  {
    pairingPasskey = 0;
  }
}
} // namespace

NimBLEServer *pServer = nullptr;
NimBLECharacteristic *pCharacteristic = nullptr;
NimBLECharacteristic *pFaceCharacteristic = nullptr;
NimBLECharacteristic *pTemperatureCharacteristic = nullptr;
NimBLECharacteristic *pConfigCharacteristic = nullptr;
NimBLECharacteristic *pCommandCharacteristic = nullptr;
NimBLECharacteristic *pTemperatureLogsCharacteristic = nullptr;
NimBLECharacteristic *pBrightnessCharacteristic = nullptr;
NimBLECharacteristic *pLuxCharacteristic = nullptr;
NimBLECharacteristic *pScrollTextCharacteristic = nullptr;
NimBLECharacteristic *pStaticColorCharacteristic = nullptr;
NimBLECharacteristic *pStrobeSettingsCharacteristic = nullptr;
NimBLECharacteristic *pOtaCharacteristic = nullptr;
NimBLECharacteristic *pInfoCharacteristic = nullptr;

std::string fwVersion = FIRMWARE_VERSION;
std::string deviceModel = DEVICE_MODEL;
std::string appCompat = APP_COMPAT_VERSION;
std::string buildTime = std::string(BUILD_DATE) + " " + BUILD_TIME;

OTACallbacks otaCallbacks;
ServerCallbacks serverCallbacks;
ScrollTextCallbacks scrollTextCallbacks;

PairingSnapshot getPairingSnapshot()
{
  PairingSnapshot snapshot{};
  portENTER_CRITICAL(&pairingMux);
  snapshot.pairing = devicePairing;
  snapshot.passkeyValid = pairingPasskeyValid;
  snapshot.passkey = pairingPasskey;
  snapshot.resetPending = pairingResetPending;
  portEXIT_CRITICAL(&pairingMux);
  return snapshot;
}

void setPairingState(bool pairing, bool passkeyValid, uint32_t passkey, bool updatePasskey)
{
  portENTER_CRITICAL(&pairingMux);
  devicePairing = pairing || pairingModeActive;
  pairingPasskeyValid = pairing && passkeyValid;
  if (updatePasskey)
  {
    pairingPasskey = pairingPasskeyValid ? passkey : 0;
  }
  portEXIT_CRITICAL(&pairingMux);
}

// cppcheck-suppress unusedFunction
void setPairingResetPending(bool pending)
{
  portENTER_CRITICAL(&pairingMux);
  pairingResetPending = pending;
  portEXIT_CRITICAL(&pairingMux);
}

// cppcheck-suppress unusedFunction
bool isPairingResetPending()
{
  bool pending = false;
  portENTER_CRITICAL(&pairingMux);
  pending = pairingResetPending;
  portEXIT_CRITICAL(&pairingMux);
  return pending;
}

void startPairingMode(unsigned long durationMs)
{
  const unsigned long nowMs = millis();
  portENTER_CRITICAL(&pairingMux);
  pairingModeActive = true;
  pairingModeDeadlineMs = nowMs + durationMs;
  devicePairing = true;
  pairingPasskeyValid = false;
  pairingPasskey = 0;
  portEXIT_CRITICAL(&pairingMux);
}

void stopPairingMode(bool clearPasskey)
{
  portENTER_CRITICAL(&pairingMux);
  pairingModeActive = false;
  pairingModeDeadlineMs = 0;
  clearPairingStateLocked(false, clearPasskey);
  portEXIT_CRITICAL(&pairingMux);
}

bool expirePairingModeIfNeeded()
{
  bool expired = false;
  const unsigned long nowMs = millis();

  portENTER_CRITICAL(&pairingMux);
  if (pairingModeActive &&
      !deviceConnected &&
      !pairingPasskeyValid &&
      hasDeadlineElapsed(nowMs, pairingModeDeadlineMs))
  {
    pairingModeActive = false;
    pairingModeDeadlineMs = 0;
    clearPairingStateLocked(false, true);
    expired = true;
  }
  portEXIT_CRITICAL(&pairingMux);

  return expired;
}

bool isPairingModeActive()
{
  expirePairingModeIfNeeded();

  bool active = false;
  portENTER_CRITICAL(&pairingMux);
  active = pairingModeActive;
  portEXIT_CRITICAL(&pairingMux);
  return active;
}

bool shouldBleAdvertise()
{
  if (deviceConnected)
  {
    return false;
  }

  if (NimBLEDevice::getNumBonds() > 0)
  {
    return true;
  }

  return isPairingModeActive();
}

void refreshBleAdvertising()
{
  NimBLEAdvertising *advertising = NimBLEDevice::getAdvertising();
  if (advertising == nullptr)
  {
    return;
  }

  if (shouldBleAdvertise())
  {
    NimBLEDevice::startAdvertising();
    return;
  }

  advertising->stop();
}
