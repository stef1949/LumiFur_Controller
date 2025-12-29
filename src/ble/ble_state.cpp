#include "ble/ble_state.h"
#include "ble/ble_constants.h"

#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"

bool deviceConnected = false;
bool oldDeviceConnected = false;
bool devicePairing = false;
uint32_t pairingPasskey = 0;
bool pairingPasskeyValid = false;
static bool pairingResetPending = false;

static portMUX_TYPE pairingMux = portMUX_INITIALIZER_UNLOCKED;

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
  devicePairing = pairing;
  pairingPasskeyValid = passkeyValid;
  if (updatePasskey)
  {
    pairingPasskey = passkey;
  }
  portEXIT_CRITICAL(&pairingMux);
}

void setPairingResetPending(bool pending)
{
  portENTER_CRITICAL(&pairingMux);
  pairingResetPending = pending;
  portEXIT_CRITICAL(&pairingMux);
}

bool isPairingResetPending()
{
  bool pending = false;
  portENTER_CRITICAL(&pairingMux);
  pending = pairingResetPending;
  portEXIT_CRITICAL(&pairingMux);
  return pending;
}
