#include "ble/ble_ota.h"
#include "ble/ble_worker.h"
#include "core/PerfTelemetry.h"

#include <Arduino.h>
#include <esp_system.h>
#include <cstring>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void OTACallbacks::onWrite(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &connInfo)
{
    (void)connInfo;
    ScopedPerfTelemetryDuration perfScope(PerfDurationId::BleCallback);

    const auto &value = pCharacteristic->getValue();
    if (value.size() == 0)
    {
        Serial.println("OTA: Received empty packet");
        return;
    }

    if (!bleQueuePayload(BleWorkType::OtaPacket, pCharacteristic,
                         reinterpret_cast<const uint8_t *>(value.data()), value.length()))
    {
        Serial.println("OTA: Worker queue full, dropping packet.");
        uint8_t response[] = {0xFF, 0x0A};
        pCharacteristic->setValue(response, sizeof(response));
        pCharacteristic->notify();
    }
}

bool OTACallbacks::processPacket(NimBLECharacteristic *pCharacteristic, const uint8_t *data, size_t len)
{
    if (data == nullptr || len == 0)
    {
        Serial.println("OTA: Received empty packet");
        return false;
    }

    const uint8_t command = data[0];

    if (command == 0x01)
    { // START OTA
        if (ota_started)
        {
            Serial.println("OTA: Already started, ignoring START command.");
            // Notify client of error if needed
            uint8_t response[] = {0xFF, 0x01}; // Error: Already started
            pCharacteristic->setValue(response, sizeof(response));
            pCharacteristic->notify();
            return false;
        }
        if (len < 5)
        {
            Serial.println("OTA: START command too short for size.");
            uint8_t response[] = {0xFF, 0x02}; // Error: Start packet too short
            pCharacteristic->setValue(response, sizeof(response));
            pCharacteristic->notify();
            return false;
        }
        memcpy(&total_size, &data[1], sizeof(total_size));
        Serial.printf("OTA: Starting OTA. Total size: %d bytes\n", total_size);

        update_partition = esp_ota_get_next_update_partition(NULL);
        if (update_partition == NULL)
        {
            Serial.println("OTA: No valid OTA partition found");
            uint8_t response[] = {0xFF, 0x03}; // Error: No partition
            pCharacteristic->setValue(response, sizeof(response));
            pCharacteristic->notify();
            return false;
        }
        Serial.printf("OTA: Writing to partition subtype %d at offset 0x%x\n",
                      update_partition->subtype, update_partition->address);

        esp_err_t err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle); // Or pass total_size if known and validated
        if (err != ESP_OK)
        {
            Serial.printf("OTA: esp_ota_begin failed (%s)\n", esp_err_to_name(err));
            uint8_t response[] = {0xFF, 0x04}; // Error: ota_begin failed
            pCharacteristic->setValue(response, sizeof(response));
            pCharacteristic->notify();
            return false;
        }
        ota_started = true;
        bytes_received = 0;
        Serial.println("OTA: esp_ota_begin succeeded. Ready for data.");
        uint8_t response[] = {0x01, 0x00}; // ACK: OTA Started
        pCharacteristic->setValue(response, sizeof(response));
        pCharacteristic->notify();
        return true;
    }
    if (command == 0x02)
    { // DATA
        if (!ota_started)
        {
            Serial.println("OTA: Received DATA before START.");
            uint8_t response[] = {0xFF, 0x05}; // Error: Data before start
            pCharacteristic->setValue(response, sizeof(response));
            pCharacteristic->notify();
            return false;
        }
        if (len <= 1)
        {
            Serial.println("OTA: DATA packet has no payload.");
            return false;
        }

        esp_err_t err = esp_ota_write(ota_handle, &data[1], len - 1);
        if (err != ESP_OK)
        {
            Serial.printf("OTA: esp_ota_write failed (%s)\n", esp_err_to_name(err));
            // Consider aborting OTA here
            uint8_t response[] = {0xFF, 0x06}; // Error: ota_write failed
            pCharacteristic->setValue(response, sizeof(response));
            pCharacteristic->notify();
            esp_ota_abort(ota_handle);
            ota_started = false;
            return false;
        }
        bytes_received += (len - 1);
// Periodically notify progress if desired
#if DEBUG_MODE
        Serial.printf("OTA: Received %d / %d bytes\n", bytes_received, total_size);
#endif
        if (bytes_received % (1024 * 10) == 0)
        { // Notify every 10KB for example
            Serial.printf("OTA: Progress %d / %d bytes\n", bytes_received, total_size);
        }
        return true;
    }
    if (command == 0x03)
    { // END OTA
        if (!ota_started)
        {
            Serial.println("OTA: Received END before START.");
            uint8_t response[] = {0xFF, 0x07}; // Error: End before start
            pCharacteristic->setValue(response, sizeof(response));
            pCharacteristic->notify();
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
            uint8_t response[] = {0xFF, 0x08}; // Error: ota_end failed
            pCharacteristic->setValue(response, sizeof(response));
            pCharacteristic->notify();
            ota_started = false; // Reset state
            return false;
        }

        err = esp_ota_set_boot_partition(update_partition);
        if (err != ESP_OK)
        {
            Serial.printf("OTA: esp_ota_set_boot_partition failed (%s)!\n", esp_err_to_name(err));
            uint8_t response[] = {0xFF, 0x09}; // Error: set_boot failed
            pCharacteristic->setValue(response, sizeof(response));
            pCharacteristic->notify();
            ota_started = false; // Reset state
            return false;
        }

        Serial.println("OTA: Update successful! Rebooting...");
        uint8_t response[] = {0x03, 0x00}; // ACK: OTA Success
        pCharacteristic->setValue(response, sizeof(response));
        pCharacteristic->notify();
        ota_started = false;
        vTaskDelay(pdMS_TO_TICKS(100)); // Give time for BLE notification to send
        esp_restart();
        return true;
    }
    if (command == 0x04)
    { // ABORT OTA
        Serial.println("OTA: ABORT command received.");
        if (ota_started)
        {
            esp_ota_abort(ota_handle);
            Serial.println("OTA: esp_ota_abort called.");
            ota_started = false;
            uint8_t response[] = {0x04, 0x00}; // ACK: OTA Aborted
            pCharacteristic->setValue(response, sizeof(response));
            pCharacteristic->notify();
            return true;
        }
        Serial.println("OTA: Abort called but OTA not started.");
        return false;
    }

    Serial.printf("OTA: Unknown command: 0x%02X\n", command);
    return false;
}

bool OTACallbacks::isActive() const
{
    return ota_started;
}
