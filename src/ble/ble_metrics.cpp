#include "ble/ble_metrics.h"
#include "ble/ble_state.h"

#include "driver/temp_sensor.h"

#include <Arduino.h>
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
static const unsigned long temperatureInterval = 5000; // 1 second interval for temperature update

// Buffer
#define HISTORY_BUFFER_SIZE 50
static float temperatureHistory[HISTORY_BUFFER_SIZE];
static int historyWriteIndex = 0;
static int validHistoryCount = 0; // Track how many valid entries are in buffer
static bool bufferFull = false;   // Flag to know when buffer has wrapped around

// Define packet structure constants
static const uint8_t PKT_TYPE_HISTORY = 0x01;
static const uint8_t MAX_FLOATS_PER_CHUNK = 4; // Adjust based on MTU (4 floats = 16 bytes)
static const size_t CHUNK_DATA_SIZE = sizeof(float) * MAX_FLOATS_PER_CHUNK;
static const size_t CHUNK_HEADER_SIZE = 3; // Type (1) + Index (1) + Total (1)
static const size_t MAX_CHUNK_PAYLOAD_SIZE = CHUNK_HEADER_SIZE + CHUNK_DATA_SIZE;

static void storeTemperatureInHistory(float temp)
{
    temperatureHistory[historyWriteIndex] = temp;
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
    memset(temperatureHistory, 0, sizeof(temperatureHistory));
    Serial.println("Temperature history buffer cleared.");
}

void triggerHistoryTransfer()
{
    if (!deviceConnected || pTemperatureLogsCharacteristic == nullptr)
    {
        Serial.println("Cannot send history: Not connected or characteristic is null.");
        return;
    }

    Serial.printf("Starting history transfer. Total valid points: %d\n", validHistoryCount);

    if (validHistoryCount == 0)
    {
        Serial.println("History buffer is empty.");
        // Optional: Send an empty marker packet? Or just do nothing.
        return;
    }

    // Calculate total chunks needed
    uint8_t totalChunks = (validHistoryCount + MAX_FLOATS_PER_CHUNK - 1) / MAX_FLOATS_PER_CHUNK;

    Serial.printf("Calculated total chunks: %d\n", totalChunks);

    std::vector<uint8_t> chunkBuffer(MAX_CHUNK_PAYLOAD_SIZE);
    int pointsSent = 0;
    int currentBufferIndex = bufferFull ? historyWriteIndex : 0; // Start reading from the oldest entry

    for (uint8_t chunkIndex = 0; chunkIndex < totalChunks; ++chunkIndex)
    {
        chunkBuffer[0] = PKT_TYPE_HISTORY;
        chunkBuffer[1] = chunkIndex;
        chunkBuffer[2] = totalChunks;

        size_t floatsInThisChunk = 0;
        size_t dataPayloadOffset = CHUNK_HEADER_SIZE;

        // Gather data for this chunk
        for (int i = 0; i < MAX_FLOATS_PER_CHUNK && pointsSent < validHistoryCount; ++i)
        {
            // Read from the circular buffer
            float tempValue = temperatureHistory[currentBufferIndex];

            // Copy float bytes into the chunk buffer
            memcpy(&chunkBuffer[dataPayloadOffset], &tempValue, sizeof(float));

            dataPayloadOffset += sizeof(float);
            floatsInThisChunk++;
            pointsSent++;

            // Increment circular buffer read index
            currentBufferIndex++;
            if (currentBufferIndex >= HISTORY_BUFFER_SIZE)
            {
                currentBufferIndex = 0; // Wrap around
            }
        }

        // Calculate the actual size of this specific chunk
        size_t currentChunkSize = CHUNK_HEADER_SIZE + (floatsInThisChunk * sizeof(float));

        // Set value and notify
        pTemperatureLogsCharacteristic->setValue(chunkBuffer.data(), currentChunkSize);
        pTemperatureLogsCharacteristic->notify();

        Serial.printf("Sent history chunk %d/%d (%d floats, %d bytes)\n",
                      chunkIndex + 1, totalChunks, floatsInThisChunk, currentChunkSize);

        // IMPORTANT: Delay between notifications to allow stacks to process
        delay(20); // Adjust delay as needed (10-50ms is typical)
    }

    Serial.println("Finished sending history chunks.");
}

void updateTemperature()
{
    unsigned long currentMillis = millis();

    // Check if enough time has passed to update temperature
    if (currentMillis - temperatureMillis >= temperatureInterval)
    {
        temperatureMillis = currentMillis;

        // Verify that the device is connected and the temperature characteristic pointer is valid.
        if (deviceConnected && pTemperatureCharacteristic != nullptr)
        {
            float result = 0;
            temp_sensor_read_celsius(&result);

            storeTemperatureInHistory(result);

            // Convert the float value to an integer
            // float temperature = (float)result;

            // Convert temperature to string using an integer value and send over BLE
            char tempStr[12];                                     // Buffer size
            snprintf(tempStr, sizeof(tempStr), "%.1f°C", result); // Format the float

            // Calculate the actual length of the formatted string (excluding potential garbage)
            size_t actualLength = strlen(tempStr);

            // Set the value using the pointer and the ACTUAL LENGTH
            pTemperatureCharacteristic->setValue(std::string(tempStr, actualLength)); // Use std::string
            pTemperatureCharacteristic->notify();

            Serial.print("Temperature: ");
            Serial.print(result);
            Serial.println(" °C");

// Print temperature values if DEBUG_MODE is enabled
#if DEBUG_MODE
            Serial.print("Temperature: ");
            Serial.print(result);
            Serial.println(" °C");
#endif
        }
        else
        {
            // Extra safeguard: log an error if pTemperatureCharacteristic is null or not connected
            if (!deviceConnected)
                Serial.println("Warning: updateTemperature called while not connected.");
            if (pTemperatureCharacteristic == nullptr)
                Serial.println("Error: pTemperatureCharacteristic is null!");
        }
    }
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
