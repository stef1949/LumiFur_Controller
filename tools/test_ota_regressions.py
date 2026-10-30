"""Compile the production OTA/worker sources against deterministic hardware fakes."""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]

FAKES = r'''
#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <string>
#include <vector>
#include <cassert>
#include <cstdio>
#include <new>
#define CONFIG_BT_NIMBLE_MAX_CONNECTIONS 3
#define CONFIG_BT_NIMBLE_PINNED_TO_CORE 1
#define DEBUG_MODE 0
#define pdMS_TO_TICKS(x) (x)
using TickType_t=uint32_t; using UBaseType_t=unsigned; using BaseType_t=int;
using TaskHandle_t=void*; using portMUX_TYPE=int;
constexpr int pdPASS=1, pdTRUE=1, portMUX_INITIALIZER_UNLOCKED=0;
constexpr uint32_t portMAX_DELAY=UINT32_MAX;
constexpr uint16_t BLE_HS_CONN_HANDLE_NONE=UINT16_MAX;
#define portENTER_CRITICAL(x) ((void)(x))
#define portEXIT_CRITICAL(x) ((void)(x))
namespace fake {
struct StopWorker {}; struct Restarted {}; struct Deleted {};
inline uint32_t now=100;
inline unsigned begins=0,writes=0,aborts=0,restarts=0,deletes=0;
inline int writeError=0,endError=0,beginError=0,bootError=0;
inline void (*worker)(void*)=nullptr;
inline void (*reboot)(void*)=nullptr;
struct Queue {size_t capacity=0,size=0;std::vector<std::vector<uint8_t>> items;};
inline Queue queue;
}
inline uint32_t millis(){return fake::now;}
inline uint32_t micros(){return fake::now*1000;}
struct Logger {void println(const char*){} template<class...T> void printf(const char*,T...){} };
inline Logger Serial;
inline void vTaskDelay(uint32_t ticks){fake::now+=ticks;}
inline void vTaskDelete(void*){++fake::deletes;throw fake::Deleted{};}
inline void esp_restart(){++fake::restarts;throw fake::Restarted{};}
inline BaseType_t xTaskCreate(void(*fn)(void*),const char*,uint32_t,void*,unsigned,void*){fake::reboot=fn;return pdPASS;}
inline BaseType_t xTaskCreatePinnedToCore(void(*fn)(void*),const char*,uint32_t,void*,unsigned,TaskHandle_t* handle,int){fake::worker=fn;*handle=(void*)1;return pdPASS;}
using QueueHandle_t=fake::Queue*;
inline QueueHandle_t xQueueCreate(size_t cap,size_t size){fake::queue={cap,size,{}};return &fake::queue;}
inline int xQueueSend(QueueHandle_t q,const void* data,uint32_t){if(q->items.size()==q->capacity)return 0;const auto*p=(const uint8_t*)data;q->items.emplace_back(p,p+q->size);return pdPASS;}
inline int xQueueReceive(QueueHandle_t q,void* data,uint32_t){if(q->items.empty())throw fake::StopWorker{};memcpy(data,q->items.front().data(),q->size);q->items.erase(q->items.begin());return pdPASS;}
inline unsigned uxQueueMessagesWaiting(QueueHandle_t q){return q->items.size();}
class NimBLECharacteristic {
public:
 std::string value;std::vector<std::vector<uint8_t>> notifications;std::vector<uint16_t> recipients;
 const std::string& getValue(){return value;}
 void setValue(const uint8_t* p,size_t n){value.assign((const char*)p,n);}
 bool notify(const uint8_t* p,size_t n,uint16_t h=BLE_HS_CONN_HANDLE_NONE){notifications.emplace_back(p,p+n);recipients.push_back(h);return true;}
};
struct NimBLEConnInfo {uint16_t handle=1;bool encrypted=true;bool isEncrypted(){return encrypted;}uint16_t getConnHandle(){return handle;}};
struct NimBLECharacteristicCallbacks {virtual void onWrite(NimBLECharacteristic*,NimBLEConnInfo&) {}};
using esp_ota_handle_t=unsigned;using esp_err_t=int;
constexpr esp_err_t ESP_OK=0,ESP_ERR_OTA_VALIDATE_FAILED=1;
struct esp_partition_t {uint32_t size;int subtype;unsigned address;};
inline esp_partition_t partition{2097152,0,0};
inline const esp_partition_t* esp_ota_get_next_update_partition(void*){return &partition;}
inline int esp_ota_begin(const esp_partition_t*,uint32_t,esp_ota_handle_t*h){++fake::begins;*h=1;return fake::beginError;}
inline int esp_ota_write(esp_ota_handle_t,const uint8_t*,size_t){++fake::writes;return fake::writeError;}
inline int esp_ota_end(esp_ota_handle_t){return fake::endError;}
inline int esp_ota_abort(esp_ota_handle_t){++fake::aborts;return 0;}
inline int esp_ota_set_boot_partition(const esp_partition_t*){return fake::bootError;}
inline const char* esp_err_to_name(int){return "fake error";}
enum class PerfDurationId{BleCallback,BleWorkerTaskBusy};
struct ScopedPerfTelemetryDuration{ScopedPerfTelemetryDuration(PerfDurationId){}};
inline void perfTelemetryRecordDuration(PerfDurationId,uint32_t){}
'''

TESTS = r'''
#include "ble/ble_worker.cpp"
#include "ble/ble_ota.cpp"
OTACallbacks otaCallbacks;
void bleHandleControlWork(const BleWorkItem&){}
NimBLECharacteristic characteristic;
void drain(){try{fake::worker(nullptr);}catch(fake::StopWorker&) {}}
void reset(){
 otaCallbacks.~OTACallbacks();new(&otaCallbacks)OTACallbacks;
 fake::now=100;fake::begins=fake::writes=fake::aborts=fake::restarts=fake::deletes=0;
 fake::writeError=fake::endError=fake::beginError=fake::bootError=0;
 fake::reboot=nullptr; characteristic=NimBLECharacteristic{};
 bleForgetConnection(1);bleForgetConnection(2);assert(startBleWorker());bleRegisterConnection(1);
}
void enqueue(std::initializer_list<uint8_t> bytes,uint16_t peer=1,bool encrypted=true){
 std::vector<uint8_t> packet(bytes);characteristic.setValue(packet.data(),packet.size());
 NimBLEConnInfo info;info.handle=peer;info.encrypted=encrypted;otaCallbacks.onWrite(&characteristic,info);
}
void packet(std::initializer_list<uint8_t> bytes,uint16_t peer=1){enqueue(bytes,peer);drain();}
void start(bool ack=true){if(ack)packet({1,4,0,0,0,1});else packet({1,4,0,0,0});assert(otaCallbacks.isActive());}
void expectError(uint8_t code){assert(characteristic.notifications.back()==std::vector<uint8_t>({255,code}));}

void test_success_reboots(){
 reset();start();assert(characteristic.notifications.back()==std::vector<uint8_t>({1,0,1}));
 packet({2,0xe9,0,0,0});assert(characteristic.notifications.back()==std::vector<uint8_t>({2,0,4,0,0,0}));
 packet({3});assert(characteristic.notifications.back()==std::vector<uint8_t>({3,0}));assert(fake::reboot);
 try{fake::reboot(nullptr);}catch(fake::Restarted&){}
 assert(fake::restarts==1 && fake::deletes==0);
}
void test_legacy_protocol(){
 reset();start(false);assert(characteristic.notifications.back()==std::vector<uint8_t>({1,0}));
 auto count=characteristic.notifications.size();packet({2,0xe9,0,0,0});assert(characteristic.notifications.size()==count);
 packet({3});assert(characteristic.notifications.back()==std::vector<uint8_t>({3,0}));
}
void test_disconnected_queued_start_is_discarded(){
 reset();enqueue({1,4,0,0,0,1});bleForgetConnection(1);drain();assert(fake::begins==0 && !otaCallbacks.isActive());
}
void test_reused_connection_handle_cannot_replay_stale_packets(){
 reset();enqueue({1,4,0,0,0,1});const auto old=bleConnectionFor(1);
 bleForgetConnection(1);bleRegisterConnection(1);assert(!bleConnectionIsCurrent(old));
 enqueue({1,4,0,0,0,1});drain();assert(fake::begins==1 && otaCallbacks.isActive());
}
void test_full_queue_does_not_prevent_disconnect_cleanup(){
 reset();start();for(int i=0;i<LF_BLE_WORKER_QUEUE_LENGTH;++i)assert(bleQueueEmpty(BleWorkType::Command));
 assert(!bleQueueEmpty(BleWorkType::Command));bleForgetConnection(1);drain();assert(fake::aborts==1 && !otaCallbacks.isActive());
 bleRegisterConnection(1);start();assert(fake::begins==2);
}
void test_other_peer_cannot_write_or_abort_session(){
 reset();start();bleRegisterConnection(2);packet({2,1,2,3,4},2);expectError(0x12);assert(fake::writes==0);
 packet({4},2);expectError(0x12);bleForgetConnection(2);drain();assert(fake::aborts==0 && otaCallbacks.isActive());
 assert(characteristic.recipients.back()==2);
}
void test_timeout_and_retry_across_clock_rollover(){
 reset();fake::now=UINT32_MAX-5000;start();fake::now+=14999;drain();assert(otaCallbacks.isActive());
 ++fake::now;drain();assert(!otaCallbacks.isActive() && fake::aborts==1);expectError(0x10);start();
}
void test_flash_and_size_failures_release_session(){
 reset();start();packet({2,1,2,3,4,5});expectError(0x0c);assert(!otaCallbacks.isActive());
 start();fake::writeError=1;packet({2,1});expectError(6);assert(!otaCallbacks.isActive());
 fake::writeError=0;start();packet({3});expectError(0x0d);assert(!otaCallbacks.isActive());
 start();packet({2,1,2,3,4});fake::endError=1;packet({3});expectError(8);assert(!otaCallbacks.isActive());
}
void test_encryption_and_queue_failure_are_reported(){
 reset();enqueue({1,4,0,0,0,1},1,false);expectError(15);assert(fake::begins==0);
 for(int i=0;i<LF_BLE_WORKER_QUEUE_LENGTH;++i)assert(bleQueueEmpty(BleWorkType::Command));
 enqueue({1,4,0,0,0,1});expectError(10);assert(!otaCallbacks.isActive());drain();
}
void test_start_errors_and_abort_allow_retry(){
 reset();packet({2,1});expectError(5);packet({1,0,0,0,0});expectError(2);
 fake::beginError=1;packet({1,4,0,0,0});expectError(4);assert(!otaCallbacks.isActive());
 fake::beginError=0;start();packet({4});assert(!otaCallbacks.isActive() && fake::aborts==1);start();
}
int main(){
 test_success_reboots();test_legacy_protocol();test_disconnected_queued_start_is_discarded();
 test_reused_connection_handle_cannot_replay_stale_packets();test_full_queue_does_not_prevent_disconnect_cleanup();
 test_other_peer_cannot_write_or_abort_session();test_timeout_and_retry_across_clock_rollover();
 test_flash_and_size_failures_release_session();test_encryption_and_queue_failure_are_reported();test_start_errors_and_abort_allow_retry();
 puts("10 production OTA/worker regression scenarios passed.");
}
'''


class OtaRegressionTest(unittest.TestCase):
    def test_production_ota_and_worker(self):
        with tempfile.TemporaryDirectory(prefix="lumifur-ota-") as directory:
            root = Path(directory)
            (root / "fakes.h").write_text(FAKES)
            headers = ["Arduino.h", "NimBLEDevice.h", "esp_system.h", "esp_ota_ops.h",
                       "freertos/FreeRTOS.h", "freertos/task.h", "freertos/queue.h", "core/PerfTelemetry.h"]
            for header in headers:
                path = root / header
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text('#include "fakes.h"\n')
            (root / "ble").mkdir()
            (root / "ble/ble_state.h").write_text('#include "ble/ble_ota.h"\nextern OTACallbacks otaCallbacks;\n')
            source, executable = root / "test.cpp", root / "test.exe"
            source.write_text(TESTS)
            subprocess.run(["g++", "-std=c++17", "-Wall", "-Wextra", f"-I{root}",
                            f"-I{ROOT / 'src'}", f"-I{ROOT / 'include'}", str(source), "-o", str(executable)], check=True)
            subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    unittest.main()
