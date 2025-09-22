#ifndef BLE_H
#define BLE_H

#include "main.h"
#include <NimBLEDevice.h>
#include "driver/temp_sensor.h"

// expose the userBrightness defined in main.h
extern uint8_t userBrightness;
extern bool brightnessChanged;

/////////////////////////////////////////////
/////////////////BLE CONFIG//////////////////
////////////////////////////////////////////

bool deviceConnected = false;
bool oldDeviceConnected = false;

// BLE UUIDs
#define SERVICE_UUID                    "01931c44-3867-7740-9867-c822cb7df308"
#define CHARACTERISTIC_UUID             "01931c44-3867-7427-96ab-8d7ac0ae09fe"
#define CONFIG_CHARACTERISTIC_UUID      "01931c44-3867-7427-96ab-8d7ac0ae09ff"
#define TEMPERATURE_CHARACTERISTIC_UUID "01931c44-3867-7b5d-9774-18350e3e27db"
#define TEMPERATURE_LOGS_CHARACTERISTIC_UUID   "0195eec2-ae6e-74a1-bcd5-215e2365477c" // New Command UUID
#define COMMAND_CHARACTERISTIC_UUID     "0195eec3-06d2-7fd4-a561-49493be3ee41"
#define BRIGHTNESS_CHARACTERISTIC_UUID  "01931c44-3867-7427-96ab-8d7ac0ae09ef"
#define OTA_CHARACTERISTIC_UUID         "01931c44-3867-7427-96ab-8d7ac0ae09ee"

//#define ULTRASOUND_CHARACTERISTIC_UUID  "01931c44-3867-7b5d-9732-12460e3a35db"

//#define DESC_USER_DESC_UUID  0x2901  // User Description descriptor
//#define DESC_FORMAT_UUID     0x2904  // Presentation Format descriptor

/*
// New BLE task to run on the NimBLE stack core
void bleTask(void *parameters) {
    for(;;) {
        // BLE events are handled internally.
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}
*/

// BLE Server pointers
NimBLEServer* pServer = nullptr;
NimBLECharacteristic* pCharacteristic = nullptr;
NimBLECharacteristic* pFaceCharacteristic = nullptr;
NimBLECharacteristic* pTemperatureCharacteristic = nullptr;
NimBLECharacteristic* pConfigCharacteristic = nullptr;
NimBLECharacteristic* pCommandCharacteristic;
NimBLECharacteristic* pTemperatureLogsCharacteristic = nullptr; // New Command UUID
NimBLECharacteristic* pBrightnessCharacteristic = nullptr;
NimBLECharacteristic* pOtaCharacteristic = nullptr;

void triggerHistoryTransfer();
void clearHistoryBuffer();
void storeTemperatureInHistory(float temp); // Good practice to declare static/helper functions if only used in .cpp
void updateTemperature();

// Class to handle BLE server callbacks
class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override {
        deviceConnected = true;
        Serial.printf("Client connected: %s\n", connInfo.getAddress().toString().c_str());

        /**
         *  We can use the connection handle here to ask for different connection parameters.
         *  Args: connection handle, min connection interval, max connection interval
         *  latency, supervision timeout.
         *  Units; Min/Max Intervals: 1.25 millisecond increments.
         *  Latency: number of intervals allowed to skip.
         *  Timeout: 10 millisecond increments.
         */
        pServer->updateConnParams(connInfo.getConnHandle(), 24, 48, 0, 180);
    }

    void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override {
        deviceConnected = false;
        Serial.println("Client disconnected - advertising");
        NimBLEDevice::startAdvertising();
    }

    void onMTUChange(uint16_t MTU, NimBLEConnInfo& connInfo) override {
        Serial.printf("MTU updated: %u for connection ID: %u\n", MTU, connInfo.getConnHandle());
    }

    /********************* Security handled here *********************/
    uint32_t onPassKeyDisplay() override {
        Serial.printf("Server Passkey Display\n");
        /**
         * This should return a random 6 digit number for security
         *  or make your own static passkey as done here.
         */
        return 123456;
    }
    
    void onConfirmPassKey(NimBLEConnInfo& connInfo, uint32_t pass_key) override {
        Serial.printf("The passkey YES/NO number: %" PRIu32 "\n", pass_key);
        /** Inject false if passkeys don't match. */
        NimBLEDevice::injectConfirmPasskey(connInfo, true);
    }

    void onAuthenticationComplete(NimBLEConnInfo& connInfo) override {
        if (!connInfo.isEncrypted()) {
            Serial.printf("Encryption not established for: %s\n", connInfo.getAddress().toString().c_str());
            // Instead of disconnecting, you might choose to leave the connection or handle it gracefully.
            // For production use you can decide to force disconnect once you’re sure your client supports pairing.
            return;
        }
        Serial.printf("Secured connection to: %s\n", connInfo.getAddress().toString().c_str());
    }

} serverCallbacks;

//Temp Non-Blocking Variables
unsigned long temperatureMillis = 0;
const unsigned long temperatureInterval = 5000; // 1 second interval for temperature update

// Buffer
#define HISTORY_BUFFER_SIZE 50
float temperatureHistory[HISTORY_BUFFER_SIZE];
int historyWriteIndex = 0;
int validHistoryCount = 0; // Track how many valid entries are in buffer
bool bufferFull = false;    // Flag to know when buffer has wrapped around

// Define packet structure constants
const uint8_t PKT_TYPE_HISTORY = 0x01;
const uint8_t MAX_FLOATS_PER_CHUNK = 4; // Adjust based on MTU (4 floats = 16 bytes)
const size_t CHUNK_DATA_SIZE = sizeof(float) * MAX_FLOATS_PER_CHUNK;
const size_t CHUNK_HEADER_SIZE = 3; // Type (1) + Index (1) + Total (1)
const size_t MAX_CHUNK_PAYLOAD_SIZE = CHUNK_HEADER_SIZE + CHUNK_DATA_SIZE;

// Definition of the helper function (can be above or below updateTemperature)
void storeTemperatureInHistory(float temp) {
    temperatureHistory[historyWriteIndex] = temp;
    historyWriteIndex++;
    if (!bufferFull) {
         validHistoryCount++;
    }
    if (historyWriteIndex >= HISTORY_BUFFER_SIZE) {
        historyWriteIndex = 0; // Wrap around
        bufferFull = true;
    }
}

void triggerHistoryTransfer() {
    if (!deviceConnected || pTemperatureLogsCharacteristic == nullptr) {
        Serial.println("Cannot send history: Not connected or characteristic is null.");
        return;
    }

    Serial.printf("Starting history transfer. Total valid points: %d\n", validHistoryCount);

    if (validHistoryCount == 0) {
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

    for (uint8_t chunkIndex = 0; chunkIndex < totalChunks; ++chunkIndex) {
        chunkBuffer[0] = PKT_TYPE_HISTORY;
        chunkBuffer[1] = chunkIndex;
        chunkBuffer[2] = totalChunks;

        size_t floatsInThisChunk = 0;
        size_t dataPayloadOffset = CHUNK_HEADER_SIZE;

        // Gather data for this chunk
        for (int i = 0; i < MAX_FLOATS_PER_CHUNK && pointsSent < validHistoryCount; ++i) {
            // Read from the circular buffer
            float tempValue = temperatureHistory[currentBufferIndex];

            // Copy float bytes into the chunk buffer
            memcpy(&chunkBuffer[dataPayloadOffset], &tempValue, sizeof(float));

            dataPayloadOffset += sizeof(float);
            floatsInThisChunk++;
            pointsSent++;

            // Increment circular buffer read index
            currentBufferIndex++;
            if (currentBufferIndex >= HISTORY_BUFFER_SIZE) {
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


void updateTemperature() {
    unsigned long currentMillis = millis();

    // Check if enough time has passed to update temperature
    if (currentMillis - temperatureMillis >= temperatureInterval) {
        temperatureMillis = currentMillis;

        // Verify that the device is connected and the temperature characteristic pointer is valid.
        if (deviceConnected && pTemperatureCharacteristic != nullptr) {
            float result = 0;
            temp_sensor_read_celsius(&result);

            storeTemperatureInHistory(result);

            // Convert the float value to an integer
            //float temperature = (float)result;

            // Convert temperature to string using an integer value and send over BLE
            char tempStr[12]; // Buffer size
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
        } else {
            // Extra safeguard: log an error if pTemperatureCharacteristic is null or not connected
            if (!deviceConnected) Serial.println("Warning: updateTemperature called while not connected.");
            if (pTemperatureCharacteristic == nullptr) Serial.println("Error: pTemperatureCharacteristic is null!");
        }
    }
}

void checkBrightness()
{
    if (brightnessChanged)
    {
 if (pBrightnessCharacteristic != nullptr) { // Null check
    uint8_t v = userBrightness;
            pBrightnessCharacteristic->setValue(&v, 1);
            pBrightnessCharacteristic->notify();
            Serial.printf("Manual brightness change (from ESP32) notified to app: %u\n", v);
        }
    }
}

#endif /* BLE_H */