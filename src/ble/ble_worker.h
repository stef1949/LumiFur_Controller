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
  AutoBrightnessFloorWrite,
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
  uint16_t connectionHandle = BLE_HS_CONN_HANDLE_NONE;
  uint32_t connectionGeneration = 0;
  uint8_t data[LF_BLE_WORK_ITEM_PAYLOAD_MAX] = {};
};

bool bleQueuePayload(BleWorkType type, NimBLECharacteristic *characteristic, const uint8_t *data, size_t length);
bool bleQueueString(BleWorkType type, NimBLECharacteristic *characteristic, const std::string &value);
bool bleQueueByte(BleWorkType type, NimBLECharacteristic *characteristic, uint8_t value);
bool bleQueueEmpty(BleWorkType type, NimBLECharacteristic *characteristic = nullptr);
bool bleIsOtaBusy();

struct BleConnectionId
{
  BleConnectionId(uint16_t h = BLE_HS_CONN_HANDLE_NONE, uint32_t g = 0) : handle(h), generation(g) {}
  uint16_t handle;
  uint32_t generation;
};

struct BleWorkQueueStats
{
  uint32_t drops = 0;
  uint32_t highWaterDepth = 0;
};

extern TaskHandle_t bleWorkerTaskHandle;
bool startBleWorker();
BleWorkQueueStats bleTakeQueueStats();
uint32_t bleQueueDepth();
void bleHandleControlWork(const BleWorkItem &item);
void bleRegisterConnection(uint16_t handle);
void bleForgetConnection(uint16_t handle);
BleConnectionId bleConnectionFor(uint16_t handle);
bool bleConnectionIsCurrent(BleConnectionId connection);
bool bleQueueOtaPacket(NimBLECharacteristic *characteristic, const uint8_t *data, size_t length, uint16_t handle);

#endif // BLE_WORKER_H
