#ifndef BLE_SERVER_H
#define BLE_SERVER_H

#include <NimBLEDevice.h>
#include <inttypes.h>

class ServerCallbacks : public NimBLEServerCallbacks
{
public:
    void onConnect(NimBLEServer *pServer, NimBLEConnInfo &connInfo) override;
    void onPairingRequest(NimBLEServer *pServer, NimBLEConnInfo &connInfo) override;
    void onDisconnect(NimBLEServer *pServer, NimBLEConnInfo &connInfo, int reason) override;
    void onMTUChange(uint16_t MTU, NimBLEConnInfo &connInfo) override;
    uint32_t onPassKeyDisplay() override;
    void onConfirmPassKey(NimBLEConnInfo &connInfo, uint32_t pass_key) override;
    void onAuthenticationComplete(NimBLEConnInfo &connInfo) override;
};

#endif // BLE_SERVER_H
