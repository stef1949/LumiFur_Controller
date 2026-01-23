#include "ble/ble_metrics.h"
#include "ble/ble_state.h"

#include "driver/temp_sensor.h"

#include <Arduino.h>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <string>
#include <vector>

extern uint8_t userBrightness;
extern bool brightnessChanged;

// Lux monitoring variables
static unsigned long luxMillis = 0;
static const unsigned long luxInterval = 500; // Update lux every 500ms
static uint16_t lastSentLuxValue = 0;
static const uint16_t luxChangeThreshold = 10; // Only send updates if lux changes by more than this amount

// Temp Non-Blocking Variables
static unsigned long temperatureMillis = 0;
static const unsigned long temperatureInterval = 1000; // 1 second interval for temperature update

static unsigned long lastHistoryTransferMillis = 0;
static const unsigned long historyTransferCooldownMs = 1500;

const bool tempNotifyEnabled = true;

// Buffer (store fixed10 samples to match Swift decoding)
#define HISTORY_BUFFER_SIZE 50
static int16_t temperatureHistoryFixed10[HISTORY_BUFFER_SIZE];
static int historyWriteIndex = 0;
static int validHistoryCount = 0; // Track how many valid entries are in buffer
static bool bufferFull = false;   // Flag to know when buffer has wrapped around

// --- History protocol (matches Swift) ---
static const uint8_t HIST_PKT_START = 0xA0;
static const uint8_t HIST_PKT_DATA  = 0xA1;
static const uint8_t HIST_PKT_END   = 0xA2;
static const uint8_t HIST_VERSION   = 0x01;

// fixed10 samples per DATA chunk.
// Keep modest; BLE notify payload depends on MTU.
// Header is 4 bytes; 16 samples = 32 bytes payload; total 36 bytes.
static const uint8_t HIST_SAMPLES_PER_CHUNK = 16;

static inline void write_u16_le(uint8_t* out, uint16_t v) {
    out[0] = (uint8_t)(v & 0xFF);
    out[1] = (uint8_t)((v >> 8) & 0xFF);
}

static inline int16_t temp_to_fixed10(float c) {
    int32_t v = lroundf(c * 10.0f);
    if (v > INT16_MAX) v = INT16_MAX;
    if (v < INT16_MIN) v = INT16_MIN;
    return (int16_t)v;
}

// If you want a known sampling interval for history timestamps (Swift uses it):
// Put your real sampling period here. If you sample every 5 seconds, set 5.
// If you later move to 1 second, set 1.
static const uint8_t HISTORY_INTERVAL_SECONDS = 1;

static void storeTemperatureInHistory(float tempC)
{
    // Convert once and store in the ring buffer as fixed10 (int16 little-endian on the wire)
    temperatureHistoryFixed10[historyWriteIndex] = temp_to_fixed10(tempC);

    historyWriteIndex++;
    if (!bufferFull)
    {
        validHistoryCount++;
    }
    if (historyWriteIndex >= HISTORY_BUFFER_SIZE)
    {
        historyWriteIndex = 0; // Wrap around
        bufferFull = true;
    }
}

void clearHistoryBuffer()
{
    historyWriteIndex = 0;
    validHistoryCount = 0;
    bufferFull = false;
    memset(temperatureHistoryFixed10, 0, sizeof(temperatureHistoryFixed10));
    Serial.println("Temperature history buffer cleared.");
}

static bool historyTransferInProgress = false;

void triggerHistoryTransfer()
{
    if (historyTransferInProgress)
    {
        Serial.println("History transfer already in progress; ignoring trigger.");
        return;
    }
    historyTransferInProgress = true;

    // Ensure we always clear the flag on exit
    struct _HistoryTransferGuard {
        bool* flag;
        ~_HistoryTransferGuard() { if (flag) *flag = false; }
    } _guard{ &historyTransferInProgress };

    const unsigned long now = millis();
    if (now - lastHistoryTransferMillis < historyTransferCooldownMs)
    {
        Serial.println("History transfer requested too soon; cooling down.");
        return;
    }
    lastHistoryTransferMillis = now;

    if (!deviceConnected || pTemperatureLogsCharacteristic == nullptr)
    {
        Serial.println("Cannot send history: Not connected or characteristic is null.");
        return;
    }

    Serial.printf("Starting history transfer. Total valid points: %d\n", validHistoryCount);

    if (validHistoryCount == 0)
    {
        Serial.println("History buffer is empty.");
        return;
    }

    // Chunks are fixed10 int16 samples. Swift expects:
    // START: [0]=0xA0 [1]=version [2]=intervalSeconds [3]=reserved [4..5]=totalPoints(u16 LE) [6..7]=startAgeMinutes(u16 LE)
    // DATA : [0]=0xA1 [1]=chunkIndex [2]=totalChunks [3]=pointsInChunk [4..]=pointsInChunk * int16 LE
    // END  : [0]=0xA2

    const uint16_t totalPoints = (uint16_t)validHistoryCount;
    const uint8_t intervalSeconds = HISTORY_INTERVAL_SECONDS;

    // Age of oldest point in minutes (rounded). Oldest sample is (totalPoints-1) intervals ago.
    const uint32_t oldestAgeSeconds = (totalPoints > 0) ? (uint32_t)(totalPoints - 1) * (uint32_t)intervalSeconds : 0;
    uint16_t startAgeMinutes = (uint16_t)((oldestAgeSeconds + 30) / 60); // round-to-nearest minute

    const uint8_t totalChunks = (uint8_t)((validHistoryCount + HIST_SAMPLES_PER_CHUNK - 1) / HIST_SAMPLES_PER_CHUNK);

    Serial.printf("History: interval=%us totalPoints=%u totalChunks=%u startAgeMinutes=%u\n",
                  intervalSeconds, totalPoints, totalChunks, startAgeMinutes);

    // ---- START packet ----
    uint8_t startPkt[8];
    startPkt[0] = HIST_PKT_START;
    startPkt[1] = HIST_VERSION;
    startPkt[2] = intervalSeconds;
    startPkt[3] = 0x00; // reserved
    write_u16_le(&startPkt[4], totalPoints);
    write_u16_le(&startPkt[6], startAgeMinutes);

    pTemperatureLogsCharacteristic->setValue(startPkt, sizeof(startPkt));
    pTemperatureLogsCharacteristic->notify();
    delay(20);

    // Oldest entry index in ring buffer
    int readIndex = bufferFull ? historyWriteIndex : 0;
    int pointsSent = 0;

    // Each DATA packet max size: 4-byte header + (HIST_SAMPLES_PER_CHUNK * 2)
    const size_t maxDataLen = 4 + (size_t)HIST_SAMPLES_PER_CHUNK * sizeof(int16_t);
    std::vector<uint8_t> pkt(maxDataLen);

    for (uint8_t chunkIndex = 0; chunkIndex < totalChunks; ++chunkIndex)
    {
        const int remaining = validHistoryCount - pointsSent;
        const uint8_t pointsInChunk = (uint8_t)((remaining > (int)HIST_SAMPLES_PER_CHUNK) ? HIST_SAMPLES_PER_CHUNK : remaining);

        pkt[0] = HIST_PKT_DATA;
        pkt[1] = chunkIndex;
        pkt[2] = totalChunks;
        pkt[3] = pointsInChunk;

        // Payload begins at byte 4
        for (uint8_t i = 0; i < pointsInChunk; ++i)
        {
            const int16_t fixed10 = temperatureHistoryFixed10[readIndex];

            // Write little-endian int16
            const size_t off = 4 + (size_t)i * 2;
            pkt[off + 0] = (uint8_t)(fixed10 & 0xFF);
            pkt[off + 1] = (uint8_t)((fixed10 >> 8) & 0xFF);

            // advance ring buffer
            readIndex++;
            if (readIndex >= HISTORY_BUFFER_SIZE) readIndex = 0;
        }

        const size_t pktLen = 4 + (size_t)pointsInChunk * 2;
        pTemperatureLogsCharacteristic->setValue(pkt.data(), pktLen);
        pTemperatureLogsCharacteristic->notify();

        pointsSent += pointsInChunk;

        Serial.printf("Sent history DATA chunk %u/%u (points=%u, bytes=%u)\n",
                      (unsigned)(chunkIndex + 1), (unsigned)totalChunks,
                      (unsigned)pointsInChunk, (unsigned)pktLen);

        delay(20);
    }

    // ---- END packet (optional; Swift currently ignores but harmless) ----
    uint8_t endPkt[1] = { HIST_PKT_END };
    pTemperatureLogsCharacteristic->setValue(endPkt, sizeof(endPkt));
    pTemperatureLogsCharacteristic->notify();

    historyTransferInProgress = false;

    Serial.println("Finished sending history chunks.");
}

void updateTemperature()
{
    const unsigned long now = millis();
    if (now - temperatureMillis < temperatureInterval) return;
    temperatureMillis = now;

    float result = 0.0f;
    // If your API returns an error code, handle it. If it doesn't, ignore this.
    // esp_err_t err = temp_sensor_read_celsius(&result);
    // if (err != ESP_OK) return;
    temp_sensor_read_celsius(&result);

    // ✅ Always record history, even if phone isn't connected.
    storeTemperatureInHistory(result);

    // Only notify if connected + subscribed
    if (!deviceConnected || !tempNotifyEnabled || pTemperatureCharacteristic == nullptr) {
        return;
    }

    const int16_t fixed10 = temp_to_fixed10(result);

    uint8_t b[2];
    b[0] = (uint8_t)(fixed10 & 0xFF);
    b[1] = (uint8_t)((fixed10 >> 8) & 0xFF);

    pTemperatureCharacteristic->setValue(b, sizeof(b));
    pTemperatureCharacteristic->notify();

#if DEBUG_MODE
    Serial.printf("Temp fixed10=%d -> %.1f°C bytes=%02X %02X\n",
                  fixed10, fixed10 / 10.0f, b[0], b[1]);
#endif
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
            extern uint16_t getAmbientLuxU16(); // Function from main.h
            uint16_t currentLux = getAmbientLuxU16();

            // Only send update if lux has changed significantly to avoid spam
            if (abs(static_cast<int>(currentLux) - static_cast<int>(lastSentLuxValue)) >= luxChangeThreshold)
            {
                // Send as 2-byte value (uint16_t)
                uint8_t luxBytes[2];
                luxBytes[0] = currentLux & 0xFF;        // Low byte
                luxBytes[1] = (currentLux >> 8) & 0xFF; // High byte

                pLuxCharacteristic->setValue(luxBytes, 2);
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

void checkBrightness()
{
    if (brightnessChanged)
    {
        if (pBrightnessCharacteristic != nullptr)
        { // Null check
            uint8_t v = userBrightness;
            pBrightnessCharacteristic->setValue(&v, 1);
            pBrightnessCharacteristic->notify();
            Serial.printf("Manual brightness change (from ESP32) notified to app: %u\n", v);
        }
    }
}
