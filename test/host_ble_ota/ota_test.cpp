#include "ble/ble_ota.h"
#include "ble/ble_worker.h"
#include "core/PerfTelemetry.h"
#include "freertos/task.h"
#include <cassert>
#include <cstring>
#include <deque>
#include <iostream>

SerialStub Serial;
static esp_partition_t partition;
static std::deque<BleWorkItem> work;
static std::vector<uint8_t> image;
static int begins, aborts, ends, boots, restarts;
static bool queueFull, noPartition, taskCreateFails;
static esp_err_t beginError, writeError, endError, bootError;
static TaskFunction_t rebootTask;
struct Restarted {};
struct DeletedTask {};

void perfTelemetryRecordDuration(PerfDurationId, uint32_t) {}
const esp_partition_t* esp_ota_get_next_update_partition(const esp_partition_t*) { return noPartition ? nullptr : &partition; }
esp_err_t esp_ota_begin(const esp_partition_t*, size_t, esp_ota_handle_t* h) { ++begins; *h = 1; image.clear(); return beginError; }
esp_err_t esp_ota_write(esp_ota_handle_t, const void* p, size_t n) {
    if (!writeError) image.insert(image.end(), static_cast<const uint8_t*>(p), static_cast<const uint8_t*>(p) + n);
    return writeError;
}
esp_err_t esp_ota_end(esp_ota_handle_t) { ++ends; return endError; }
esp_err_t esp_ota_abort(esp_ota_handle_t) { ++aborts; return ESP_OK; }
esp_err_t esp_ota_set_boot_partition(const esp_partition_t*) { ++boots; return bootError; }
void esp_restart() { ++restarts; throw Restarted{}; }
void vTaskDelay(TickType_t ticks) { assert(ticks >= 250); }
void vTaskDelete(void*) { throw DeletedTask{}; }
BaseType_t xTaskCreate(TaskFunction_t task, const char*, uint32_t, void*, UBaseType_t, void*) {
    if (taskCreateFails) return 0;
    rebootTask = task;
    return pdPASS;
}
bool bleQueueOtaPacket(NimBLECharacteristic* c, const uint8_t* p, size_t n, uint32_t token) {
    if (queueFull) return false;
    BleWorkItem item{};
    item.type = BleWorkType::OtaPacket; item.characteristic = c;
    item.length = n; item.otaSessionToken = token;
    std::memcpy(item.data, p, n); work.push_back(item); return true;
}
bool bleQueueEmpty(BleWorkType type, NimBLECharacteristic*) {
    if (queueFull) return false;
    BleWorkItem item{}; item.type = type; work.push_back(item); return true;
}
static void drain(OTACallbacks& ota) {
    while (!work.empty()) {
        const auto item = work.front(); work.pop_front();
        ota.cleanupDisconnectedSession();
        if (item.type == BleWorkType::OtaPacket)
            ota.processPacket(item.characteristic, item.data, item.length, item.otaSessionToken);
    }
}
static void write(OTACallbacks& ota, NimBLECharacteristic& c, std::vector<uint8_t> bytes, uint16_t handle = 7, bool encrypted = true) {
    c.value.assign(bytes.begin(), bytes.end());
    NimBLEConnInfo info{handle, encrypted}; ota.onWrite(&c, info);
}
static void send(OTACallbacks& ota, NimBLECharacteristic& c, std::vector<uint8_t> bytes) {
    write(ota, c, bytes); drain(ota);
}
static void response(const NimBLECharacteristic& c, uint8_t code, uint8_t detail, uint16_t handle = 7) {
    assert(!c.notifications.empty());
    const auto& n = c.notifications.back();
    assert(n.bytes == std::vector<uint8_t>({code, detail})); assert(n.handle == handle);
}
static void reset() {
    begins = aborts = ends = boots = restarts = 0;
    beginError = writeError = endError = bootError = 0;
    queueFull = noPartition = taskCreateFails = false;
    rebootTask = nullptr; work.clear(); image.clear();
}
static void success(bool extended) {
    reset(); OTACallbacks ota; NimBLECharacteristic c;
    auto start = std::vector<uint8_t>{1, 3, 0, 0, 0}; if (extended) start.push_back(1);
    write(ota, c, start); assert(begins == 0 && c.notifications.empty());
    drain(ota); response(c, 1, extended ? 1 : 0); assert(ota.isActive());
    send(ota, c, {2, 0xE9, 2, 3});
    assert(image == std::vector<uint8_t>({0xE9, 2, 3}));
    response(c, extended ? 2 : 1, 0);
    send(ota, c, {3}); response(c, 3, 0);
    assert(!ota.isActive() && boots == 1 && rebootTask && restarts == 0);
    try { rebootTask(nullptr); } catch (const Restarted&) {}
    assert(restarts == 1);
    send(ota, c, start); response(c, 0xFF, 1); // No second update while rebooting.
}
int main() {
    success(false); success(true);
    { reset(); OTACallbacks ota; NimBLECharacteristic c;
      send(ota, c, {1, 3, 0, 0, 0, 1}); write(ota, c, {2, 0xE9});
      ota.onDisconnect(7); drain(ota);
      assert(!ota.isActive() && aborts == 1 && image.empty());
      send(ota, c, {1, 3, 0, 0, 0, 1}); assert(ota.isActive() && begins == 2);
      send(ota, c, {4}); response(c, 4, 0); send(ota, c, {4}); response(c, 4, 0);
    }
    { reset(); OTACallbacks ota; NimBLECharacteristic c;
      write(ota, c, {1, 3, 0, 0, 0}); ota.onDisconnect(7);
      write(ota, c, {1, 2, 0, 0, 0}); drain(ota);
      assert(begins == 1 && ota.isActive()); // Reused handle cannot revive queued START.
      send(ota, c, {2, 0xE9, 1}); send(ota, c, {3}); assert(boots == 1);
    }
    { reset(); OTACallbacks ota; NimBLECharacteristic c;
      send(ota, c, {1, 3, 0, 0, 0});
      write(ota, c, {2, 0xFF}, 8); response(c, 0xFF, 0x10, 8);
      ota.onDisconnect(8); drain(ota); assert(ota.isActive() && aborts == 0);
      write(ota, c, {4}, 8); response(c, 0xFF, 0x10, 8);
    }
    { reset(); OTACallbacks ota; NimBLECharacteristic c;
      send(ota, c, {1, 3, 0, 0, 0}); write(ota, c, {2, 0xE9});
      queueFull = true; write(ota, c, {2, 1}); response(c, 0xFF, 0x0A);
      queueFull = false; drain(ota); assert(aborts == 1 && !ota.isActive() && image.empty());
      send(ota, c, {1, 3, 0, 0, 0}); assert(begins == 2);
    }
    { reset(); OTACallbacks ota; NimBLECharacteristic c;
      send(ota, c, {1, 3, 0, 0, 0}); send(ota, c, {2, 1, 2, 3, 4});
      response(c, 0xFF, 0x0C); assert(aborts == 1 && !ota.isActive() && boots == 0);
      send(ota, c, {1, 3, 0, 0, 0}); send(ota, c, {2, 1}); send(ota, c, {3});
      response(c, 0xFF, 0x0D); assert(aborts == 2 && ends == 0 && boots == 0);
    }
    for (int failure = 0; failure < 5; ++failure) {
      reset(); OTACallbacks ota; NimBLECharacteristic c;
      noPartition = failure == 0; beginError = failure == 1;
      writeError = failure == 2; endError = failure == 3; bootError = failure == 4;
      send(ota, c, {1, 1, 0, 0, 0});
      if (failure >= 2) send(ota, c, {2, 0xE9});
      if (failure >= 3) send(ota, c, {3});
      assert(!ota.isActive() && !rebootTask && restarts == 0);
      assert(aborts == (failure == 2 ? 1 : 0)); // esp_ota_end frees its handle even on failure.
      noPartition = false; beginError = writeError = endError = bootError = 0;
      send(ota, c, {1, 1, 0, 0, 0}); assert(ota.isActive());
    }
    { reset(); OTACallbacks ota; NimBLECharacteristic c;
      write(ota, c, {1, 1, 0, 0, 0}, 7, false); response(c, 0xFF, 0x0F); assert(work.empty());
      for (auto bytes : {std::vector<uint8_t>{1}, std::vector<uint8_t>{1, 0, 0, 0, 0}, std::vector<uint8_t>{1, 1, 0, 0, 0, 2}}) {
          send(ota, c, bytes); response(c, 0xFF, 2); assert(!ota.isActive());
      }
      send(ota, c, {1, 1, 0, 0, 0}); assert(ota.isActive());
    }
    { reset(); OTACallbacks ota; NimBLECharacteristic c; taskCreateFails = true;
      send(ota, c, {1, 1, 0, 0, 0}); send(ota, c, {2, 0xE9});
      try { send(ota, c, {3}); } catch (const Restarted&) {}
      assert(restarts == 1 && boots == 1);
    }
    std::cout << "PASS: OTA legacy/acknowledged uploads, restart, disconnect, ownership, queue overflow, size checks, flash failures, retry, malformed input\n";
}
