#ifndef BLE_H
#define BLE_H

#include "main.h"
#include <NimBLEDevice.h>
#include "driver/temp_sensor.h"
#include <esp_ota_ops.h> // For OTA_SIZE_UNKNOWN
#include "core/ScrollState.h"

// expose the userBrightness defined in main.h
extern uint8_t userBrightness;
extern bool brightnessChanged;

/////////////////////////////////////////////
/////////////////BLE CONFIG//////////////////
////////////////////////////////////////////

bool deviceConnected = false;
bool oldDeviceConnected = false;
bool devicePairing = false;


// BLE UUIDs
#define SERVICE_UUID "01931c44-3867-7740-9867-c822cb7df308"
#define CHARACTERISTIC_UUID "01931c44-3867-7427-96ab-8d7ac0ae09fe"
#define CONFIG_CHARACTERISTIC_UUID "01931c44-3867-7427-96ab-8d7ac0ae09ff"
#define TEMPERATURE_CHARACTERISTIC_UUID "01931c44-3867-7b5d-9774-18350e3e27db"
#define TEMPERATURE_LOGS_CHARACTERISTIC_UUID "0195eec2-ae6e-74a1-bcd5-215e2365477c" // New Command UUID
#define COMMAND_CHARACTERISTIC_UUID "0195eec3-06d2-7fd4-a561-49493be3ee41"
#define BRIGHTNESS_CHARACTERISTIC_UUID "01931c44-3867-7427-96ab-8d7ac0ae09ef"

#define SCROLL_TEXT_CHARACTERISTIC_UUID "7f9b8b12-1234-4c55-9b77-a19d55aa0011"
#define STATIC_COLOR_CHARACTERISTIC_UUID "7f9b8b12-1234-4c55-9b77-a19d55aa0022"
#define LUX_CHARACTERISTIC_UUID "01931c44-3867-7427-96ab-8d7ac0ae09f0"

#define OTA_CHARACTERISTIC_UUID "01931c44-3867-7427-96ab-8d7ac0ae09ee"
#define INFO_CHARACTERISTIC_UUID "cba1d466-344c-4be3-ab3f-189f80dd7599"

// #define ULTRASOUND_CHARACTERISTIC_UUID  "01931c44-3867-7b5d-9732-12460e3a35db"

// #define DESC_USER_DESC_UUID  0x2901  // User Description descriptor
// #define DESC_FORMAT_UUID     0x2904  // Presentation Format descriptor

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
NimBLEServer *pServer = nullptr;
NimBLECharacteristic *pCharacteristic = nullptr;
NimBLECharacteristic *pFaceCharacteristic = nullptr;
NimBLECharacteristic *pTemperatureCharacteristic = nullptr;
NimBLECharacteristic *pConfigCharacteristic = nullptr;
NimBLECharacteristic *pCommandCharacteristic;
NimBLECharacteristic *pTemperatureLogsCharacteristic = nullptr; // New Command UUID
NimBLECharacteristic *pBrightnessCharacteristic = nullptr;
NimBLECharacteristic *pLuxCharacteristic = nullptr;

NimBLECharacteristic *pScrollTextCharacteristic = nullptr; // NEW
NimBLECharacteristic *pStaticColorCharacteristic = nullptr;

NimBLECharacteristic *pOtaCharacteristic = nullptr;
static NimBLECharacteristic *pInfoCharacteristic;

void triggerHistoryTransfer();
void clearHistoryBuffer();
void storeTemperatureInHistory(float temp); // Good practice to declare static/helper functions if only used in .cpp
void updateTemperature();
void updateLux(); // NEW: Add lux function declaration

// Fallback defines in case PlatformIO doesn't inject them
#ifndef FIRMWARE_VERSION
// #define FIRMWARE_VERSION "unknown"
#define FIRMWARE_VERSION "3.0.0" // Default version if not defined
#endif

#ifndef GIT_COMMIT
#define GIT_COMMIT "unknown"
#endif

#ifndef GIT_BRANCH
#ifdef DEBUG_MODE
#define GIT_BRANCH "dev"
#else
#define GIT_BRANCH "main"
#endif
#endif

#ifndef BUILD_DATE
#define BUILD_DATE __DATE__
#endif

#ifndef BUILD_TIME
#define BUILD_TIME __TIME__
#endif

#ifndef DEVICE_MODEL
#define DEVICE_MODEL "mps3"
#endif

#ifndef APP_COMPAT_VERSION
#define APP_COMPAT_VERSION "6.1"
#endif

std::string fwVersion = FIRMWARE_VERSION;
std::string deviceModel = DEVICE_MODEL;
std::string appCompat = APP_COMPAT_VERSION;
std::string buildTime = std::string(BUILD_DATE) + " " + BUILD_TIME;

class OTACallbacks : public NimBLECharacteristicCallbacks
{
    esp_ota_handle_t ota_handle = 0;
    const esp_partition_t *update_partition = nullptr;
    int bytes_received = 0;
    int total_size = 0;
    bool ota_started = false;

public:
    void onWrite(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &connInfo) override
    {
        std::string value = pCharacteristic->getValue();
        const uint8_t *data = (const uint8_t *)value.data();
        size_t len = value.length();

        if (len == 0)
        {
            Serial.println("OTA: Received empty packet");
            return;
        }

        uint8_t command = data[0];

        if (command == 0x01)
        { // START OTA
            if (ota_started)
            {
                Serial.println("OTA: Already started, ignoring START command.");
                // Notify client of error if needed
                uint8_t response[] = {0xFF, 0x01}; // Error: Already started
                pCharacteristic->setValue(response, sizeof(response));
                pCharacteristic->notify();
                return;
            }
            if (len < 5)
            {
                Serial.println("OTA: START command too short for size.");
                uint8_t response[] = {0xFF, 0x02}; // Error: Start packet too short
                pCharacteristic->setValue(response, sizeof(response));
                pCharacteristic->notify();
                return;
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
                return;
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
                return;
            }
            ota_started = true;
            bytes_received = 0;
            Serial.println("OTA: esp_ota_begin succeeded. Ready for data.");
            uint8_t response[] = {0x01, 0x00}; // ACK: OTA Started
            pCharacteristic->setValue(response, sizeof(response));
            pCharacteristic->notify();
        }
        else if (command == 0x02)
        { // DATA
            if (!ota_started)
            {
                Serial.println("OTA: Received DATA before START.");
                uint8_t response[] = {0xFF, 0x05}; // Error: Data before start
                pCharacteristic->setValue(response, sizeof(response));
                pCharacteristic->notify();
                return;
            }
            if (len <= 1)
            {
                Serial.println("OTA: DATA packet has no payload.");
                return; // Or send error
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
                return;
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
        }
        else if (command == 0x03)
        { // END OTA
            if (!ota_started)
            {
                Serial.println("OTA: Received END before START.");
                uint8_t response[] = {0xFF, 0x07}; // Error: End before start
                pCharacteristic->setValue(response, sizeof(response));
                pCharacteristic->notify();
                return;
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
                return;
            }

            err = esp_ota_set_boot_partition(update_partition);
            if (err != ESP_OK)
            {
                Serial.printf("OTA: esp_ota_set_boot_partition failed (%s)!\n", esp_err_to_name(err));
                uint8_t response[] = {0xFF, 0x09}; // Error: set_boot failed
                pCharacteristic->setValue(response, sizeof(response));
                pCharacteristic->notify();
                ota_started = false; // Reset state
                return;
            }

            Serial.println("OTA: Update successful! Rebooting...");
            uint8_t response[] = {0x03, 0x00}; // ACK: OTA Success
            pCharacteristic->setValue(response, sizeof(response));
            pCharacteristic->notify();
            delay(100); // Give time for BLE notification to send
            esp_restart();
        }
        else if (command == 0x04)
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
            }
            else
            {
                Serial.println("OTA: Abort called but OTA not started.");
            }
        }
        else
        {
            Serial.printf("OTA: Unknown command: 0x%02X\n", command);
        }
    }
};
static OTACallbacks otaCallbacks; // Instantiate the callbacks
// This is a global instance of the OTACallbacks class to handle OTA updates

// Class to handle BLE server callbacks
class ServerCallbacks : public NimBLEServerCallbacks
{
    void onConnect(NimBLEServer *pServer, NimBLEConnInfo &connInfo) override
    {
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
    void onPairingRequest(NimBLEServer *pServer, NimBLEConnInfo &connInfo)
    {
        devicePairing = true;
        Serial.printf("Pairing request from: %s\n", connInfo.getAddress().toString().c_str());
    }
    void onDisconnect(NimBLEServer *pServer, NimBLEConnInfo &connInfo, int reason) override
    {
        deviceConnected = false;
        Serial.println("Client disconnected - advertising");
        NimBLEDevice::startAdvertising();
    }

    void onMTUChange(uint16_t MTU, NimBLEConnInfo &connInfo) override
    {
        Serial.printf("MTU updated: %u for connection ID: %u\n", MTU, connInfo.getConnHandle());
    }

    /********************* Security handled here *********************/
    uint32_t onPassKeyDisplay() override
    {
        Serial.printf("Server Passkey Display\n");
        /**
         * This should return a random 6 digit number for security
         *  or make your own static passkey as done here.
         */
        return 123456;
    }

    // uint32_t onPassKeyRequest()
    // {
    //     Serial.printf("Server Passkey Request\n");
    //     /**
    //      * This should return a random 6 digit number for security
    //      *  or make your own static passkey as done here.
    //      */
    //     return 123456;
    // }

    void onConfirmPassKey(NimBLEConnInfo &connInfo, uint32_t pass_key) override
    {
        Serial.printf("The passkey YES/NO number: %" PRIu32 "\n", pass_key);
        /** Inject false if passkeys don't match. */
        NimBLEDevice::injectConfirmPasskey(connInfo, true);
    }

    void onAuthenticationComplete(NimBLEConnInfo &connInfo) override
    {
        if (!connInfo.isEncrypted())
        {
            Serial.printf("Encryption not established for: %s\n", connInfo.getAddress().toString().c_str());
            // Instead of disconnecting, you might choose to leave the connection or handle it gracefully.
            // For production use you can decide to force disconnect once you’re sure your client supports pairing.
            return;
        }
        Serial.printf("Secured connection to: %s\n", connInfo.getAddress().toString().c_str());
    }

} serverCallbacks;
class ScrollSpeedCallbacks : public NimBLECharacteristicCallbacks
{
    void onWrite(NimBLECharacteristic *pChr, NimBLEConnInfo &connInfo) override
    {
        const auto &val = pChr->getValue(); // getValue() returns a temporary; bind as const reference
        if (val.size() >= 1)
        {
            uint8_t incoming = static_cast<uint8_t>(val[0]);
            if (incoming < 1)
                incoming = 1;
            if (incoming > 100)
                incoming = 100;
            auto &scrollState = scroll::state();
            scrollState.speedSetting = incoming;
            scroll::updateIntervalFromSpeed();
#if DEBUG_MODE
            Serial.printf("BLE: New scroll speed=%u (interval=%u ms)\n", scrollState.speedSetting, scrollState.textIntervalMs);
#endif
            // Echo back & notify (optional)
            pChr->setValue(&scrollState.speedSetting, 1);
            pChr->notify();
        }
    }
    void onRead(NimBLECharacteristic *pChr, NimBLEConnInfo &connInfo) override
    {
        auto &scrollState = scroll::state();
        pChr->setValue(&scrollState.speedSetting, 1);
    }
};
static ScrollSpeedCallbacks scrollSpeedCallbacks;

class ScrollTextCallbacks : public NimBLECharacteristicCallbacks
{
    void onWrite(NimBLECharacteristic *pChr, NimBLEConnInfo &connInfo) override
    {
        extern char txt[64]; // Declare external txt buffer
        const auto &val = pChr->getValue();
        if (val.size() > 0 && val.size() < sizeof(txt))
        {
            // Copy new message to txt buffer
            memset(txt, 0, sizeof(txt)); // Clear existing text
            memcpy(txt, val.data(), val.size());
            txt[val.size()] = '\0'; // Ensure null termination

            // Reinitialize scrolling text with new message
            extern void initializeScrollingText();
            initializeScrollingText();

#if DEBUG_MODE
            Serial.printf("BLE: New scroll text received: '%s' (%u bytes)\n", txt, val.size());
#endif

            // Echo back & notify (optional)
            pChr->setValue(txt);
            pChr->notify();
        }
        else if (val.size() >= sizeof(txt))
        {
            Serial.printf("BLE: Scroll text too long! Max %u bytes, got %u\n", sizeof(txt) - 1, val.size());
        }
    }

    void onRead(NimBLECharacteristic *pChr, NimBLEConnInfo &connInfo) override
    {
        extern char txt[64]; // ADD THIS LINE - Declare extern to access the global txt buffer
                             // Return current text when app reads
        pChr->setValue(txt);
#if DEBUG_MODE
        Serial.printf("BLE: Scroll text read: '%s'\n", txt);
#endif
    }
};

static ScrollTextCallbacks scrollTextCallbacks;

// Lux monitoring variables
unsigned long luxMillis = 0;
const unsigned long luxInterval = 500; // Update lux every 500ms
uint16_t lastSentLuxValue = 0;
const uint16_t luxChangeThreshold = 10; // Only send updates if lux changes by more than this amount

// Temp Non-Blocking Variables
unsigned long temperatureMillis = 0;
const unsigned long temperatureInterval = 5000; // 1 second interval for temperature update

// Buffer
#define HISTORY_BUFFER_SIZE 50
float temperatureHistory[HISTORY_BUFFER_SIZE];
int historyWriteIndex = 0;
int validHistoryCount = 0; // Track how many valid entries are in buffer
bool bufferFull = false;   // Flag to know when buffer has wrapped around

// Define packet structure constants
const uint8_t PKT_TYPE_HISTORY = 0x01;
const uint8_t MAX_FLOATS_PER_CHUNK = 4; // Adjust based on MTU (4 floats = 16 bytes)
const size_t CHUNK_DATA_SIZE = sizeof(float) * MAX_FLOATS_PER_CHUNK;
const size_t CHUNK_HEADER_SIZE = 3; // Type (1) + Index (1) + Total (1)
const size_t MAX_CHUNK_PAYLOAD_SIZE = CHUNK_HEADER_SIZE + CHUNK_DATA_SIZE;

// Definition of the helper function (can be above or below updateTemperature)
void storeTemperatureInHistory(float temp)
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
            // Get current lux value from the APDS9960 sensor
            extern uint16_t getRawClearChannelValue(); // Function from main.h
            uint16_t currentLux = getRawClearChannelValue();

            // Only send update if lux has changed significantly to avoid spam
            if (abs((int)currentLux - (int)lastSentLuxValue) >= luxChangeThreshold)
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

#endif /* BLE_H */
