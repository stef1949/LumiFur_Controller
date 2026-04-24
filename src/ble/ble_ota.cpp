#include "ble/ble_ota.h"
#include "ble/ble_worker.h"
#include "core/PerfTelemetry.h"

#include <Arduino.h>
#include <esp_system.h>
#include <cstring>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace
{
constexpr TickType_t kOtaRebootDelayTicks = pdMS_TO_TICKS(250);

void otaRebootTask(void *param)
{
    (void)param;
    vTaskDelay(kOtaRebootDelayTicks);
    esp_restart();
    vTaskDelete(nullptr);
}

void scheduleOtaReboot()
{
    constexpr uint32_t kTaskStackWords = 2048;
    constexpr UBaseType_t kTaskPriority = 2;
    BaseType_t created = xTaskCreate(otaRebootTask, "ota_reboot", kTaskStackWords, nullptr, kTaskPriority, nullptr);
    if (created != pdPASS)
    {
        Serial.println("OTA: Failed to schedule reboot task, rebooting immediately.");
        esp_restart();
    }
}
} // namespace

void OTACallbacks::onWrite(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &connInfo)
{
    ScopedPerfTelemetryDuration perfScope(PerfDurationId::BleCallback);

    if (pCharacteristic == nullptr)
    {
        Serial.println("OTA: Characteristic is null");
        return;
    }

    const auto &value = pCharacteristic->getValue();
    if (value.size() == 0)
    {
        Serial.println("OTA: Received empty packet");
        return;
    }

    if (!connInfo.isEncrypted())
    {
        Serial.println("OTA: Rejecting write from unencrypted connection.");
        uint8_t response[] = {0xFF, 0x0F};
        pCharacteristic->setValue(response, sizeof(response));
        pCharacteristic->notify();
        return;
    }

    if (value.size() > LF_BLE_WORK_ITEM_PAYLOAD_MAX)
    {
        Serial.printf("OTA: Packet too large (%u > %u)\n",
                      static_cast<unsigned int>(value.size()),
                      static_cast<unsigned int>(LF_BLE_WORK_ITEM_PAYLOAD_MAX));
        uint8_t response[] = {0xFF, 0x0B};
        pCharacteristic->setValue(response, sizeof(response));
        pCharacteristic->notify();
        return;
    }

    if (!bleQueuePayload(BleWorkType::OtaPacket, pCharacteristic,
                         reinterpret_cast<const uint8_t *>(value.data()), value.length()))
    {
        Serial.println("OTA: Worker queue unavailable/full, dropping packet.");
        uint8_t response[] = {0xFF, 0x0A};
        pCharacteristic->setValue(response, sizeof(response));
        pCharacteristic->notify();
    }
}

bool OTACallbacks::processPacket(NimBLECharacteristic *pCharacteristic, const uint8_t *data, size_t len)
{
    if (pCharacteristic == nullptr || data == nullptr || len == 0)
    {
        Serial.println("OTA: Received empty packet");
        return false;
    }

    auto sendResponse = [&](uint8_t code, uint8_t detail)
    {
        const uint8_t response[] = {code, detail};
        pCharacteristic->setValue(response, sizeof(response));
        pCharacteristic->notify();
    };

    auto resetSession = [&]()
    {
        ota_started.store(false, std::memory_order_relaxed);
        ota_handle = 0;
        update_partition = nullptr;
        bytes_received = 0;
        total_size = 0;
    };

    const uint8_t command = data[0];

    if (command == 0x01)
    { // START OTA
        if (ota_started.load(std::memory_order_relaxed))
        {
            Serial.println("OTA: Already started, ignoring START command.");
            sendResponse(0xFF, 0x01); // Error: Already started
            return false;
        }
        if (len < 5)
        {
            Serial.println("OTA: START command too short for size.");
            sendResponse(0xFF, 0x02); // Error: Start packet too short
            return false;
        }

        uint32_t declaredSize = 0;
        memcpy(&declaredSize, &data[1], sizeof(declaredSize));
        if (declaredSize == 0)
        {
            Serial.println("OTA: START command has invalid size 0.");
            sendResponse(0xFF, 0x02); // Error: Invalid start payload
            return false;
        }

        update_partition = esp_ota_get_next_update_partition(NULL);
        if (update_partition == NULL)
        {
            Serial.println("OTA: No valid OTA partition found");
            sendResponse(0xFF, 0x03); // Error: No partition
            return false;
        }

        if (declaredSize > update_partition->size)
        {
            Serial.printf("OTA: Declared size %u exceeds partition size %u\n",
                          static_cast<unsigned int>(declaredSize),
                          static_cast<unsigned int>(update_partition->size));
            sendResponse(0xFF, 0x03); // Error: Invalid partition/size combination
            update_partition = nullptr;
            return false;
        }

        total_size = declaredSize;
        Serial.printf("OTA: Starting OTA. Total size: %u bytes\n", static_cast<unsigned int>(total_size));
        Serial.printf("OTA: Writing to partition subtype %d at offset 0x%x\n",
                      update_partition->subtype, update_partition->address);

        esp_err_t err = esp_ota_begin(update_partition, total_size, &ota_handle);
        if (err != ESP_OK)
        {
            Serial.printf("OTA: esp_ota_begin failed (%s)\n", esp_err_to_name(err));
            sendResponse(0xFF, 0x04); // Error: ota_begin failed
            update_partition = nullptr;
            return false;
        }

        ota_started.store(true, std::memory_order_relaxed);
        bytes_received = 0;
        Serial.println("OTA: esp_ota_begin succeeded. Ready for data.");
        sendResponse(0x01, 0x00); // ACK: OTA Started
        return true;
    }
    if (command == 0x02)
    { // DATA
        if (!ota_started.load(std::memory_order_relaxed))
        {
            Serial.println("OTA: Received DATA before START.");
            sendResponse(0xFF, 0x05); // Error: Data before start
            return false;
        }
        if (len <= 1)
        {
            Serial.println("OTA: DATA packet has no payload.");
            sendResponse(0xFF, 0x0B); // Error: Invalid data payload
            return false;
        }

        const uint32_t payloadBytes = static_cast<uint32_t>(len - 1);
        if (bytes_received + payloadBytes > total_size)
        {
            Serial.printf("OTA: DATA exceeds declared size (%u + %u > %u)\n",
                          static_cast<unsigned int>(bytes_received),
                          static_cast<unsigned int>(payloadBytes),
                          static_cast<unsigned int>(total_size));
            esp_ota_abort(ota_handle);
            sendResponse(0xFF, 0x0C); // Error: Data exceeds declared size
            resetSession();
            return false;
        }

        esp_err_t err = esp_ota_write(ota_handle, &data[1], len - 1);
        if (err != ESP_OK)
        {
            Serial.printf("OTA: esp_ota_write failed (%s)\n", esp_err_to_name(err));
            sendResponse(0xFF, 0x06); // Error: ota_write failed
            esp_ota_abort(ota_handle);
            resetSession();
            return false;
        }
        bytes_received += payloadBytes;

#if DEBUG_MODE
        Serial.printf("OTA: Received %u / %u bytes\n",
                      static_cast<unsigned int>(bytes_received),
                      static_cast<unsigned int>(total_size));
#endif
        return true;
    }
    if (command == 0x03)
    { // END OTA
        if (!ota_started.load(std::memory_order_relaxed))
        {
            Serial.println("OTA: Received END before START.");
            sendResponse(0xFF, 0x07); // Error: End before start
            return false;
        }

        if (bytes_received != total_size)
        {
            Serial.printf("OTA: Size mismatch at END (%u != %u)\n",
                          static_cast<unsigned int>(bytes_received),
                          static_cast<unsigned int>(total_size));
            esp_ota_abort(ota_handle);
            sendResponse(0xFF, 0x0D); // Error: End with incomplete/extra data
            resetSession();
            return false;
        }

        Serial.println("OTA: END command received.");
        esp_err_t err = esp_ota_end(ota_handle);
        if (err != ESP_OK)
        {
            Serial.printf("OTA: esp_ota_end failed (%s)\n", esp_err_to_name(err));
            if (err == ESP_ERR_OTA_VALIDATE_FAILED)
            {
                Serial.println("OTA: Image validation failed, image is corrupted");
            }
            sendResponse(0xFF, 0x08); // Error: ota_end failed
            resetSession();
            return false;
        }

        err = esp_ota_set_boot_partition(update_partition);
        if (err != ESP_OK)
        {
            Serial.printf("OTA: esp_ota_set_boot_partition failed (%s)!\n", esp_err_to_name(err));
            sendResponse(0xFF, 0x09); // Error: set_boot failed
            resetSession();
            return false;
        }

        Serial.println("OTA: Update successful! Rebooting...");
        sendResponse(0x03, 0x00); // ACK: OTA Success
        resetSession();
        scheduleOtaReboot();
        return true;
    }
    if (command == 0x04)
    { // ABORT OTA
        Serial.println("OTA: ABORT command received.");
        if (ota_started.load(std::memory_order_relaxed))
        {
            esp_ota_abort(ota_handle);
            Serial.println("OTA: esp_ota_abort called.");
            resetSession();
            sendResponse(0x04, 0x00); // ACK: OTA Aborted
            return true;
        }
        Serial.println("OTA: Abort called but OTA not started.");
        sendResponse(0xFF, 0x07); // Error: Abort before start
        return false;
    }

    Serial.printf("OTA: Unknown command: 0x%02X\n", command);
    sendResponse(0xFF, 0x0E); // Error: Unknown command
    return false;
}

bool OTACallbacks::isActive() const
{
    return ota_started.load(std::memory_order_relaxed);
}
