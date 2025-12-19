#ifndef BLE_OTA_H
#define BLE_OTA_H

#include <NimBLEDevice.h>
#include <esp_ota_ops.h>

class OTACallbacks : public NimBLECharacteristicCallbacks
{
    esp_ota_handle_t ota_handle = 0;
    const esp_partition_t *update_partition = nullptr;
    int bytes_received = 0;
    int total_size = 0;
    bool ota_started = false;

public:
    void onWrite(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &connInfo) override;
};

#endif // BLE_OTA_H
