#ifndef BLE_METRICS_H
#define BLE_METRICS_H

void triggerHistoryTransfer();
void clearHistoryBuffer();
void updateTemperature();
void updateLux();
void requestLuxResend();

#endif // BLE_METRICS_H
