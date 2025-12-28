#include "ble/ble_server.h"
#include "ble/ble_state.h"

void ServerCallbacks::onConnect(NimBLEServer *pServer, NimBLEConnInfo &connInfo)
{
    deviceConnected = true;

    /**
     *  We can use the connection handle here to ask for different connection parameters.
     *  Args: connection handle, min connection interval, max connection interval
     *  latency, supervision timeout.
     *  Units; Min/Max Intervals: 1.25 millisecond increments.
     *  Latency: number of intervals allowed to skip.
     *  Timeout: 10 millisecond increments.
     */
    pServer->updateConnParams(connInfo.getConnHandle(), 24, 48, 0, 180);

#if DEBUG_BLE
    Serial.printf("Client connected: %s\n", connInfo.getAddress().toString().c_str());
#endif
}

void ServerCallbacks::onPairingRequest(NimBLEServer *pServer, NimBLEConnInfo &connInfo)
{
    devicePairing = true;
#if DEBUG_BLE
    Serial.printf("Pairing request from: %s\n", connInfo.getAddress().toString().c_str());
#endif
}

void ServerCallbacks::onDisconnect(NimBLEServer *pServer, NimBLEConnInfo &connInfo, int reason)
{
    deviceConnected = false;
    NimBLEDevice::startAdvertising();
#if DEBUG_BLE
    Serial.println("Client disconnected - advertising");
#endif
}

void ServerCallbacks::onMTUChange(uint16_t MTU, NimBLEConnInfo &connInfo)
{
#if DEBUG_BLE
    Serial.printf("MTU updated: %u for connection ID: %u\n", MTU, connInfo.getConnHandle());
#endif
}

uint32_t ServerCallbacks::onPassKeyDisplay()
{
    /**
     * This should return a random 6 digit number for security
     *  or make your own static passkey as done here.
     */
#if DEBUG_BLE
    Serial.printf("Server Passkey Display\n");
#endif
    return 123456;
}

void ServerCallbacks::onConfirmPassKey(NimBLEConnInfo &connInfo, uint32_t pass_key)
{
#if DEBUG_BLE
    Serial.printf("The passkey YES/NO number: %" PRIu32 "\n", pass_key);
#endif
    /** Inject false if passkeys don't match. */
    NimBLEDevice::injectConfirmPasskey(connInfo, true);
}

void ServerCallbacks::onAuthenticationComplete(NimBLEConnInfo &connInfo)
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
