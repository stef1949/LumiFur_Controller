#ifndef BLE_OTA_H
#define BLE_OTA_H

#include <NimBLEDevice.h>
#include <esp_ota_ops.h>

#include <cstddef>

class OTACallbacks : public NimBLECharacteristicCallbacks
{
    esp_ota_handle_t ota_handle = 0;
    const esp_partition_t *update_partition = nullptr;
    int bytes_received = 0;
    int total_size = 0;
    bool ota_started = false;

public:
    void onWrite(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &connInfo) override;
    bool processPacket(NimBLECharacteristic *pCharacteristic, const uint8_t *data, size_t len);
    bool isActive() const;
};

#endif // BLE_OTA_H
