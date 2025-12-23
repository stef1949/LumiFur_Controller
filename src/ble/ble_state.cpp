#include "ble/ble_state.h"
#include "ble/ble_constants.h"

bool deviceConnected = false;
bool oldDeviceConnected = false;
bool devicePairing = false;

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
