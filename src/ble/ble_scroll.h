#ifndef BLE_SCROLL_H
#define BLE_SCROLL_H

#include <NimBLEDevice.h>

class ScrollSpeedCallbacks : public NimBLECharacteristicCallbacks
{
public:
    void onWrite(NimBLECharacteristic *pChr, NimBLEConnInfo &connInfo) override;
    void onRead(NimBLECharacteristic *pChr, NimBLEConnInfo &connInfo) override;
};

class ScrollTextCallbacks : public NimBLECharacteristicCallbacks
{
public:
    void onWrite(NimBLECharacteristic *pChr, NimBLEConnInfo &connInfo) override;
    void onRead(NimBLECharacteristic *pChr, NimBLEConnInfo &connInfo) override;
};

#endif // BLE_SCROLL_H
