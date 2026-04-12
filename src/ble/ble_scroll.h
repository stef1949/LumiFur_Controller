#ifndef BLE_SCROLL_H
#define BLE_SCROLL_H

#include <NimBLEDevice.h>

#include <cstddef>
#include <cstdint>

class ScrollTextCallbacks : public NimBLECharacteristicCallbacks
{
public:
    void onWrite(NimBLECharacteristic *pChr, NimBLEConnInfo &connInfo) override;
    void onRead(NimBLECharacteristic *pChr, NimBLEConnInfo &connInfo) override;
};

bool handleScrollWritePayload(NimBLECharacteristic *pChr, const uint8_t *data, size_t length);

#endif // BLE_SCROLL_H
