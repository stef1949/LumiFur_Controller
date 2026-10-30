#pragma once
#include <cstdint>
bool bluetoothOverlayNeedsContinuousRefresh(unsigned long nowMs);
void drawBluetoothStatusIcon();
void drawPairingPasskeyOverlay(uint32_t passkey);
