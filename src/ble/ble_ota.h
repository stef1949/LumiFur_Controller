#ifndef BLE_OTA_H
#define BLE_OTA_H

#include <NimBLEDevice.h>
#include <esp_ota_ops.h>

#include <atomic>
#include <cstddef>
#include <cstdint>

class OTACallbacks : public NimBLECharacteristicCallbacks
{
    esp_ota_handle_t ota_handle = 0;
    const esp_partition_t *update_partition = nullptr;
    uint32_t bytes_received = 0;
    uint32_t total_size = 0;
    std::atomic<bool> ota_started{false};
    // The BLE callback reserves ownership before enqueueing START. Tokens distinguish
    // a reconnect from queued packets belonging to a previous connection/session.
    std::atomic<uint32_t> connectionToken{0};
    std::atomic<bool> rebootPending{false};
    uint16_t nextSessionId = 0; // BLE callback queue only
    uint32_t sessionToken = 0; // Worker task only
    bool acknowledgeChunks = false;

    void resetSession();

public:
    void onWrite(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &connInfo) override;
    bool processPacket(NimBLECharacteristic *pCharacteristic, const uint8_t *data, size_t len, uint32_t token);
    void onDisconnect(uint16_t connHandle);
    void cleanupDisconnectedSession();
    bool isActive() const;
};

#endif // BLE_OTA_H
