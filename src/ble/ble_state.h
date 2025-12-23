#ifndef BLE_STATE_H
#define BLE_STATE_H

#include <NimBLEDevice.h>
#include <string>

#include "ble/ble_ota.h"
#include "ble/ble_server.h"
#include "ble/ble_scroll.h"

extern bool deviceConnected;
extern bool oldDeviceConnected;
extern bool devicePairing;

extern NimBLEServer *pServer;
extern NimBLECharacteristic *pCharacteristic;
extern NimBLECharacteristic *pFaceCharacteristic;
extern NimBLECharacteristic *pTemperatureCharacteristic;
extern NimBLECharacteristic *pConfigCharacteristic;
extern NimBLECharacteristic *pCommandCharacteristic;
extern NimBLECharacteristic *pTemperatureLogsCharacteristic;
extern NimBLECharacteristic *pBrightnessCharacteristic;
extern NimBLECharacteristic *pLuxCharacteristic;
extern NimBLECharacteristic *pScrollTextCharacteristic;
extern NimBLECharacteristic *pStaticColorCharacteristic;
extern NimBLECharacteristic *pOtaCharacteristic;
extern NimBLECharacteristic *pInfoCharacteristic;

extern std::string fwVersion;
extern std::string deviceModel;
extern std::string appCompat;
extern std::string buildTime;

extern OTACallbacks otaCallbacks;
extern ServerCallbacks serverCallbacks;
extern ScrollTextCallbacks scrollTextCallbacks;

#endif // BLE_STATE_H
