#include <NimBLEDevice.h>

////////////////////////////////////////////
/////////////////BLE CONFIG/////////////////
////////////////////////////////////////////

bool deviceConnected = false;
bool oldDeviceConnected = false;

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
        /** Check that encryption was successful, if not we disconnect the client */
        if (!connInfo.isEncrypted()) {
            NimBLEDevice::getServer()->disconnect(connInfo.getConnHandle());
            Serial.printf("Encrypt connection failed - disconnecting client\n");
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

    if (deviceConnected) {
      // Read the ESP32's internal temperature
      float temperature = temperatureRead();  // Temperature in Celsius

      // Convert temperature to string and send over BLE
      char tempStr[12];
      snprintf(tempStr, sizeof(tempStr), "%.1f°C", temperature);
      pTemperatureCharacteristic->setValue(tempStr);
      pTemperatureCharacteristic->notify();

      // Optional: Debug output to serial
      Serial.print("Internal Temperature: ");
      Serial.println(tempStr);
    }
  }
}

