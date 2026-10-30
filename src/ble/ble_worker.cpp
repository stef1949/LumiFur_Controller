#include "ble/ble_worker.h"
#include "ble/ble_state.h"
#include "core/PerfTelemetry.h"

#include <Arduino.h>
#include <array>
#include <cstring>
#include "freertos/queue.h"

TaskHandle_t bleWorkerTaskHandle = nullptr;

namespace
{
QueueHandle_t workQueue = nullptr;
portMUX_TYPE workMux = portMUX_INITIALIZER_UNLOCKED;
BleWorkQueueStats queueStats;
std::array<BleConnectionId, CONFIG_BT_NIMBLE_MAX_CONNECTIONS> connections;
uint32_t nextGeneration = 0;

bool queueWork(const BleWorkItem &item)
{
  if (!workQueue || xQueueSend(workQueue, &item, 0) != pdPASS)
  {
    portENTER_CRITICAL(&workMux);
    ++queueStats.drops;
    portEXIT_CRITICAL(&workMux);
    return false;
  }
  const uint32_t depth = uxQueueMessagesWaiting(workQueue);
  portENTER_CRITICAL(&workMux);
  if (depth > queueStats.highWaterDepth) queueStats.highWaterDepth = depth;
  portEXIT_CRITICAL(&workMux);
  return true;
}

void workerTask(void *)
{
  BleWorkItem item;
  for (;;)
  {
    // Poll even with an empty/full queue: disconnect cleanup never depends on
    // being able to enqueue a special abort message from the BLE callback.
    otaCallbacks.poll(millis());
    if (xQueueReceive(workQueue, &item, pdMS_TO_TICKS(100)) != pdPASS) continue;
    const uint32_t started = micros();
    if (item.type == BleWorkType::OtaPacket)
    {
      const BleConnectionId connection{item.connectionHandle, item.connectionGeneration};
      if (bleConnectionIsCurrent(connection))
        otaCallbacks.processPacket(item.characteristic, item.data, item.length, connection.handle, connection.generation);
    }
    else
    {
      bleHandleControlWork(item);
    }
    perfTelemetryRecordDuration(PerfDurationId::BleWorkerTaskBusy, micros() - started);
  }
}
}

bool startBleWorker()
{
  workQueue = xQueueCreate(LF_BLE_WORKER_QUEUE_LENGTH, sizeof(BleWorkItem));
  if (!workQueue) return false;
  return xTaskCreatePinnedToCore(workerTask, "BLE Worker", LF_BLE_WORKER_STACK_SIZE,
                               nullptr, LF_BLE_WORKER_PRIORITY, &bleWorkerTaskHandle,
                               CONFIG_BT_NIMBLE_PINNED_TO_CORE) == pdPASS;
}

BleWorkQueueStats bleTakeQueueStats()
{
  portENTER_CRITICAL(&workMux);
  const auto result = queueStats;
  queueStats = {};
  portEXIT_CRITICAL(&workMux);
  return result;
}

uint32_t bleQueueDepth()
{
  return workQueue ? uxQueueMessagesWaiting(workQueue) : 0;
}

void bleRegisterConnection(uint16_t handle)
{
  portENTER_CRITICAL(&workMux);
  for (auto &connection : connections)
  {
    if (connection.handle == handle || connection.handle == BLE_HS_CONN_HANDLE_NONE)
    {
      if (++nextGeneration == 0) ++nextGeneration;
      connection.handle = handle;
      connection.generation = nextGeneration;
      break;
    }
  }
  portEXIT_CRITICAL(&workMux);
}

void bleForgetConnection(uint16_t handle)
{
  portENTER_CRITICAL(&workMux);
  for (auto &connection : connections)
    if (connection.handle == handle) connection = {};
  portEXIT_CRITICAL(&workMux);
}

BleConnectionId bleConnectionFor(uint16_t handle)
{
  BleConnectionId result;
  portENTER_CRITICAL(&workMux);
  for (const auto &connection : connections)
    if (connection.handle == handle) result = connection;
  portEXIT_CRITICAL(&workMux);
  return result;
}

bool bleConnectionIsCurrent(BleConnectionId connection)
{
  if (!connection.generation) return false;
  const auto current = bleConnectionFor(connection.handle);
  return current.handle == connection.handle && current.generation == connection.generation;
}

bool bleQueuePayload(BleWorkType type, NimBLECharacteristic *characteristic, const uint8_t *data, size_t length)
{
  if ((length && !data) || length > LF_BLE_WORK_ITEM_PAYLOAD_MAX) return false;
  BleWorkItem item;
  item.type = type;
  item.characteristic = characteristic;
  item.length = static_cast<uint16_t>(length);
  if (length) std::memcpy(item.data, data, length);
  return queueWork(item);
}

bool bleQueueOtaPacket(NimBLECharacteristic *characteristic, const uint8_t *data, size_t length, uint16_t handle)
{
  const auto connection = bleConnectionFor(handle);
  if (!connection.generation || !data || !length || length > LF_BLE_WORK_ITEM_PAYLOAD_MAX) return false;
  BleWorkItem item;
  item.type = BleWorkType::OtaPacket;
  item.characteristic = characteristic;
  item.length = static_cast<uint16_t>(length);
  item.connectionHandle = connection.handle;
  item.connectionGeneration = connection.generation;
  std::memcpy(item.data, data, length);
  return queueWork(item);
}

bool bleQueueString(BleWorkType type, NimBLECharacteristic *characteristic, const std::string &value)
{
  return bleQueuePayload(type, characteristic, reinterpret_cast<const uint8_t *>(value.data()), value.size());
}

bool bleQueueByte(BleWorkType type, NimBLECharacteristic *characteristic, uint8_t value)
{
  return bleQueuePayload(type, characteristic, &value, 1);
}

bool bleQueueEmpty(BleWorkType type, NimBLECharacteristic *characteristic)
{
  return bleQueuePayload(type, characteristic, nullptr, 0);
}

bool bleIsOtaBusy()
{
  return otaCallbacks.isActive();
}
