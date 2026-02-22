#include "ble/ble_server.h"
#include "ble/ble_state.h"

#include "esp_system.h"

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
    setPairingState(true, false, 0, false);
#if DEBUG_BLE
    Serial.printf("Pairing request from: %s\n", connInfo.getAddress().toString().c_str());
#endif
}

void ServerCallbacks::onDisconnect(NimBLEServer *pServer, NimBLEConnInfo &connInfo, int reason)
{
    deviceConnected = false;
    setPairingState(false, false, 0, true);
    const bool resetPending = isPairingResetPending();
    if (resetPending && pServer->getConnectedCount() == 0)
    {
        const bool cleared = NimBLEDevice::deleteAllBonds();
        setPairingResetPending(false);
#if DEBUG_BLE
        Serial.printf("BLE pairing reset: bonds cleared=%s\n", cleared ? "true" : "false");
#endif
    }
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
    const PairingSnapshot snapshot = getPairingSnapshot();
    const uint32_t passkey = snapshot.passkeyValid ? snapshot.passkey : (esp_random() % 1000000);
    setPairingState(true, true, passkey, true);
#if DEBUG_BLE
    Serial.printf("Server Passkey Display: %06" PRIu32 "\n", passkey);
#endif
    return passkey;
}

void ServerCallbacks::onConfirmPassKey(NimBLEConnInfo &connInfo, uint32_t pass_key)
{
#if DEBUG_BLE
    Serial.printf("The passkey YES/NO number: %" PRIu32 "\n", pass_key);
#endif
    setPairingState(true, true, pass_key, true);
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
        setPairingState(false, false, 0, true);
        return;
    }
    setPairingState(false, false, 0, true);
    Serial.printf("Secured connection to: %s\n", connInfo.getAddress().toString().c_str());
}
