#ifndef BLE_WORKER_H
#define BLE_WORKER_H

#include <NimBLEDevice.h>
#include "perf_tuning.h"

#include <cstddef>
#include <cstdint>
#include <string>

enum class BleWorkType : uint8_t
{
  Command = 0,
  FaceWrite,
  ConfigWrite,
  BrightnessWrite,
  StaticColorWrite,
  StrobeSettingsWrite,
  ScrollWrite,
  OtaPacket
};

struct BleWorkItem
{
  BleWorkType type = BleWorkType::Command;
  NimBLECharacteristic *characteristic = nullptr;
  uint16_t length = 0;
  uint8_t data[LF_BLE_WORK_ITEM_PAYLOAD_MAX] = {};
};

bool bleQueuePayload(BleWorkType type, NimBLECharacteristic *characteristic, const uint8_t *data, size_t length);
bool bleQueueString(BleWorkType type, NimBLECharacteristic *characteristic, const std::string &value);
bool bleQueueByte(BleWorkType type, NimBLECharacteristic *characteristic, uint8_t value);
bool bleQueueEmpty(BleWorkType type, NimBLECharacteristic *characteristic = nullptr);
bool bleIsOtaBusy();

#endif // BLE_WORKER_H
