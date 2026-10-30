#include "ble/ble_metrics.h"
#include "ble/ble_state.h"
#include "core/AdaptiveBrightness.h"
#include "core/InternalTemperature.h"
#include "core/TemperatureHistory.h"

#include <Arduino.h>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/portmacro.h"

namespace
{
unsigned long luxMillis = 0;
constexpr unsigned long luxInterval = 500;
uint16_t lastSentLuxValue = 0;
constexpr uint16_t luxChangeThreshold = 10;
// Set when a client (re)subscribes, so it gets the current reading even if lux
// has not moved by luxChangeThreshold since the last notification.
std::atomic<bool> luxResendRequested{true};
uint32_t temperatureMillis = 0;
uint32_t lastHistoryTransferMillis = 0;
bool historyTransferred = false;
constexpr uint32_t historyTransferCooldownMs = 1500;
telemetry::TemperatureHistory history;
portMUX_TYPE historyMux = portMUX_INITIALIZER_UNLOCKED;
}

void clearHistoryBuffer()
{
    portENTER_CRITICAL(&historyMux);
    history.clear();
    portEXIT_CRITICAL(&historyMux);
}

void triggerHistoryTransfer()
{
    // Called only by the BLE worker. No state lock is held during notifications.
    const uint32_t now = millis();
    if (historyTransferred && now - lastHistoryTransferMillis < historyTransferCooldownMs) return;
    if (!deviceConnected || !pTemperatureLogsCharacteristic) return;
    lastHistoryTransferMillis = now;
    historyTransferred = true;

    portENTER_CRITICAL(&historyMux);
    const auto snapshot = history.snapshot();
    portEXIT_CRITICAL(&historyMux);

    // Send an empty START as well, so clients can finish an empty-history request.
    uint8_t packet[telemetry::kHistoryPacketMaxBytes];
    size_t length = telemetry::encodeHistoryStart(snapshot, now, packet, sizeof(packet));
    pTemperatureLogsCharacteristic->setValue(packet, length);
    if (!pTemperatureLogsCharacteristic->notify()) return;
    vTaskDelay(pdMS_TO_TICKS(20));
    for (size_t chunk = 0; chunk < telemetry::historyChunkCount(snapshot); ++chunk)
    {
        if (!deviceConnected) return;
        length = telemetry::encodeHistoryChunk(snapshot, chunk, packet, sizeof(packet));
        pTemperatureLogsCharacteristic->setValue(packet, length);
        if (!pTemperatureLogsCharacteristic->notify()) return;
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    const uint8_t end[] = {0xA2};
    pTemperatureLogsCharacteristic->setValue(end, sizeof(end));
    pTemperatureLogsCharacteristic->notify();
}

void updateTemperature()
{
    // The Arduino loop owns sampling, in both awake and sleep states.
    const uint32_t now = millis();
    if (now - temperatureMillis < telemetry::kTemperatureIntervalMs) return;
    temperatureMillis = now;
    float celsius = 0.0f;
    if (!internalTemperatureReadCelsius(celsius) || !std::isfinite(celsius)) return;
    const int16_t fixed10 = telemetry::temperatureToFixed10(celsius);

    portENTER_CRITICAL(&historyMux);
    const auto previous = history.snapshot();
    // The wire format describes evenly spaced samples. Start a new contiguous
    // history after a sensor outage or stalled loop rather than inventing samples.
    if (previous.count && now - previous.newestSampleMs >= 2 * telemetry::kTemperatureIntervalMs) history.clear();
    history.append(fixed10, now);
    portEXIT_CRITICAL(&historyMux);

    if (pTemperatureCharacteristic)
    {
        const uint8_t bytes[] = {static_cast<uint8_t>(fixed10), static_cast<uint8_t>(static_cast<uint16_t>(fixed10) >> 8)};
        pTemperatureCharacteristic->setValue(bytes, sizeof(bytes));
        if (deviceConnected) pTemperatureCharacteristic->notify();
    }
}

void requestLuxResend()
{
    luxResendRequested = true;
}

void updateLux()
{
    unsigned long currentMillis = millis();

    // Check if enough time has passed to update lux
    if (currentMillis - luxMillis >= luxInterval)
    {
        luxMillis = currentMillis;

        // Verify that the device is connected and the lux characteristic pointer is valid.
        if (deviceConnected && pLuxCharacteristic != nullptr)
        {
            // Get current lux value from the APDS9960 sensor (in lux)
            uint16_t currentLux = getAmbientLuxU16();

            // Keep the readable value current so a client read on connect is not stale.
            uint8_t luxBytes[2];
            luxBytes[0] = currentLux & 0xFF;        // Low byte
            luxBytes[1] = (currentLux >> 8) & 0xFF; // High byte
            pLuxCharacteristic->setValue(luxBytes, 2);

            // Only notify if lux has changed significantly (or a client just subscribed) to avoid spam
            const bool resend = luxResendRequested.exchange(false);
            if (resend || abs(static_cast<int>(currentLux) - static_cast<int>(lastSentLuxValue)) >= luxChangeThreshold)
            {
                pLuxCharacteristic->notify();

                lastSentLuxValue = currentLux;

#if DEBUG_MODE
                Serial.printf("Lux level sent: %u\n", currentLux);
#endif
            }
        }
        else
        {
            // Extra safeguard: log an error if pLuxCharacteristic is null or not connected
            if (!deviceConnected)
                Serial.println("Warning: updateLux called while not connected.");
            if (pLuxCharacteristic == nullptr)
                Serial.println("Error: pLuxCharacteristic is null!");
        }
    }
}
