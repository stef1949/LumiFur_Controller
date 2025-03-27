#ifndef BLE_H
#define BLE_H

#include <NimBLEDevice.h>
#include "driver/temp_sensor.h"
////////////////////////////////////////////
//////////////////BLE UUIDs/////////////////
////////////////////////////////////////////
#define SERVICE_UUID                    "01931c44-3867-7740-9867-c822cb7df308"
#define CHARACTERISTIC_UUID             "01931c44-3867-7427-96ab-8d7ac0ae09fe"
#define CONFIG_CHARACTERISTIC_UUID      "01931c44-3867-7427-96ab-8d7ac0ae09ff"
#define TEMPERATURE_CHARACTERISTIC_UUID "01931c44-3867-7b5d-9774-18350e3e27db"
//#define ULTRASOUND_CHARACTERISTIC_UUID  "01931c44-3867-7b5d-9732-12460e3a35db"

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
//#define ULTRASOUND_CHARACTERISTIC_UUID  "01931c44-3867-7b5d-9732-12460e3a35db"

//#define DESC_USER_DESC_UUID  0x2901  // User Description descriptor
//#define DESC_FORMAT_UUID     0x2904  // Presentation Format descriptor


// BLE Server pointers
NimBLEServer* pServer = nullptr;
NimBLECharacteristic* pCharacteristic = nullptr;
NimBLECharacteristic* pFaceCharacteristic = nullptr;
NimBLECharacteristic* pTemperatureCharacteristic = nullptr;
NimBLECharacteristic* pConfigCharacteristic = nullptr;

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

void updateTemperature() {
    unsigned long currentMillis = millis();

    // Check if enough time has passed to update temperature
    if (currentMillis - temperatureMillis >= temperatureInterval) {
        temperatureMillis = currentMillis;

        // Verify that the device is connected and the temperature characteristic pointer is valid.
        if (deviceConnected && pTemperatureCharacteristic != nullptr) {
            float result = 0;
            temp_sensor_read_celsius(&result);

            // Convert the float value to an integer
            int temperature = (int)result;

            // Convert temperature to string using an integer value and send over BLE
            char tempStr[12];
            snprintf(tempStr, sizeof(tempStr), "%d°C", temperature);
            pTemperatureCharacteristic->setValue(tempStr);
            pTemperatureCharacteristic->notify();

            // Print temperature values if DEBUG_MODE is enabled
            #if DEBUG_MODE
            Serial.print("Temperature: ");
            Serial.print(result);
            Serial.println(" °C");
            #endif
        } else {
            // Extra safeguard: log an error if pTemperatureCharacteristic is null
            Serial.println("Error: pTemperatureCharacteristic is null!");
        }
    }
}

#endif /* BLE_H */