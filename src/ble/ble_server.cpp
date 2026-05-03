#include "ble/ble_server.h"
#include "ble/ble_state.h"

#include <Arduino.h>
#include <array>

#include "esp_system.h"

namespace
{
bool isBondedPeer(const NimBLEConnInfo &connInfo)
{
    if (connInfo.isBonded())
    {
        return true;
    }

    const NimBLEAddress peerId = connInfo.getIdAddress();
    if (!peerId.isNull() && NimBLEDevice::isBonded(peerId))
    {
        return true;
    }

    const NimBLEAddress peerAddr = connInfo.getAddress();
    return !peerAddr.isNull() && NimBLEDevice::isBonded(peerAddr);
}

constexpr size_t kMaxPairingCandidates = CONFIG_BT_NIMBLE_MAX_CONNECTIONS;
std::array<uint16_t, kMaxPairingCandidates> pairingCandidateHandles = {};
size_t pairingCandidateCount = 0;

void addPairingCandidate(uint16_t connHandle)
{
    for (size_t index = 0; index < pairingCandidateCount; ++index)
    {
        if (pairingCandidateHandles[index] == connHandle)
        {
            return;
        }
    }

    if (pairingCandidateCount >= pairingCandidateHandles.size())
    {
#if DEBUG_BLE
        Serial.printf("Pairing candidate table full, dropping handle %u\n", connHandle);
#endif
        return;
    }

    pairingCandidateHandles[pairingCandidateCount++] = connHandle;
}

bool removePairingCandidate(uint16_t connHandle)
{
    for (size_t index = 0; index < pairingCandidateCount; ++index)
    {
        if (pairingCandidateHandles[index] != connHandle)
        {
            continue;
        }

        for (size_t shift = index + 1; shift < pairingCandidateCount; ++shift)
        {
            pairingCandidateHandles[shift - 1] = pairingCandidateHandles[shift];
        }
        --pairingCandidateCount;
        pairingCandidateHandles[pairingCandidateCount] = 0;
        return true;
    }

    return false;
}
} // namespace

void ServerCallbacks::onConnect(NimBLEServer *pServer, NimBLEConnInfo &connInfo)
{
    const bool bondedPeer = isBondedPeer(connInfo);
    if (!bondedPeer && !isPairingModeActive())
    {
#if DEBUG_BLE
        Serial.printf("Rejecting BLE connection from unpaired device: %s\n", connInfo.getAddress().toString().c_str());
#endif
        pServer->disconnect(connInfo.getConnHandle());
        return;
    }

    if (!bondedPeer)
    {
        addPairingCandidate(connInfo.getConnHandle());
    }

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

void ServerCallbacks::onDisconnect(NimBLEServer *pServer, NimBLEConnInfo &connInfo, int reason)
{
    removePairingCandidate(connInfo.getConnHandle());
    deviceConnected = pServer->getConnectedCount() > 0;
    setPairingState(false, false, 0, true);
    const bool resetPending = isPairingResetPending();
    if (resetPending && pServer->getConnectedCount() == 0)
    {
#if DEBUG_BLE
        const bool cleared = NimBLEDevice::deleteAllBonds();
        Serial.printf("BLE pairing reset: bonds cleared=%s\n", cleared ? "true" : "false");
#else
        NimBLEDevice::deleteAllBonds();
#endif
        setPairingResetPending(false);
    }
    refreshBleAdvertising();
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
    const bool pairedDuringThisSession = removePairingCandidate(connInfo.getConnHandle());

    if (!connInfo.isEncrypted())
    {
        Serial.printf("Encryption not established for: %s\n", connInfo.getAddress().toString().c_str());
        setPairingState(false, false, 0, true);
        if (pServer != nullptr)
        {
            pServer->disconnect(connInfo.getConnHandle());
        }
        return;
    }

    if (pairedDuringThisSession)
    {
        stopPairingMode(true);
    }

    setPairingState(false, false, 0, true);
    Serial.printf("Secured connection to: %s\n", connInfo.getAddress().toString().c_str());
}
