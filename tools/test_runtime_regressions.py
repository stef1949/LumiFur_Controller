"""Exercise the production input/power/settings/telemetry controllers on the host."""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]

STUB = r'''
#include <cstdint>
#include <cstdio>
#include <cassert>
#include <vector>
#include <functional>
#include <cmath>
#include <cstdlib>
#include "app/Views.h"
#include "app/ViewState.h"
#include "app/PowerManagement.h"
#include "app/InputController.h"
#include "core/InputState.h"
#include "core/PowerPolicy.h"
#include "core/TemperatureHistory.h"
#include "core/AnimationState.h"
#define DEBUG_MODE 0
#define BUTTON_UP 1
#define BUTTON_DOWN 2
#define LOW 0
#define pdMS_TO_TICKS(x) (x)
#define portMUX_INITIALIZER_UNLOCKED 0
using portMUX_TYPE=int;
#define portENTER_CRITICAL(x) ((void)(x))
#define portEXIT_CRITICAL(x) ((void)(x))
uint32_t clockMs=0;
unsigned frequency=240, refreshes=0, persisted=0, temperatureReads=0;
bool keys[3]={}, activity=false, otaActive=false, deviceConnected=false;
uint32_t millis(){return clockMs;}
int digitalRead(int pin){return keys[pin]?0:1;}
void vTaskDelay(uint32_t){}
void setCpuFrequencyMhz(unsigned value){frequency=value;}
struct {void println(const char*){} template<class...T> void printf(const char*,T...){} } Serial;
struct AppStateLock {};
struct CRGB {uint8_t r=0,g=0,b=0;static const CRGB Green;}; const CRGB CRGB::Green{};
struct FakeCharacteristic {
 std::vector<uint8_t> value;
 std::vector<std::vector<uint8_t>> notifications;
 std::function<void()> onNotify;
 void setValue(const uint8_t*p,size_t n){value.assign(p,p+n);}
 bool notify(){notifications.push_back(value);if(onNotify)onNotify();return true;}
} face, temperature, historyCharacteristic;
auto pFaceCharacteristic=&face;
auto pTemperatureCharacteristic=&temperature;
auto pTemperatureLogsCharacteristic=&historyCharacteristic;
FakeCharacteristic* pLuxCharacteristic=nullptr;
struct Advertising {void stop(){}void setMinInterval(int){}void setMaxInterval(int){}} advertising;
struct NimBLEDevice {static Advertising* getAdvertising(){return &advertising;}};
void refreshBleAdvertising(){}
void requestDisplayRefresh(){++refreshes;}
void notifyBleTask(){}
void updateGlobalBrightnessScale(uint8_t){}
void syncBrightnessState(uint8_t){}
bool viewUsesMic(int){return true;}
void micSetEnabled(bool){}
bool sleepMotionDetected(){return activity;}
bool bleIsOtaBusy(){return otaActive;}
CRGB getStaticColorCached(){return {};}
void noteButtonInteraction(unsigned long){}
void resetBlePairing(){}
void startPairingMode(unsigned long){}
void scheduleLastViewPersist(uint8_t view){persisted=view;}
bool internalTemperatureReadCelsius(float &out){++temperatureReads;out=25.3f;return true;}
uint16_t getAmbientLuxU16(){return 0;}
'''

TESTS = r'''
int main(){
 // Sleep is entered without an accelerometer, but a debounced local button
 // remains available and the actual view-selection path restores controls.
 sleepModeEnabled=true;accelerometerEnabled=false;lastActivityTime=0;
 clockMs=SLEEP_TIMEOUT_MS+1;checkSleepMode();assert(sleepModeActive && frequency==80);
 keys[BUTTON_DOWN]=true;handleButtonInputs(clockMs);clockMs+=50;handleButtonInputs(clockMs);
 assert(!sleepModeActive && frequency==240 && lastActivityTime==clockMs);
 keys[BUTTON_DOWN]=false;clockMs+=50;handleButtonInputs(clockMs);clockMs+=50;handleButtonInputs(clockMs);
 assert(persisted==userSelectedView);

 enterSleepMode();sleepModeEnabled=false;applyConfigOptions();assert(!sleepModeActive && frequency==240);
 sleepModeEnabled=true;enterSleepMode();otaActive=true;checkSleepMode();assert(!sleepModeActive);
 clockMs+=SLEEP_TIMEOUT_MS+1;checkSleepMode();assert(!sleepModeActive);otaActive=false;

 // A user selection cancels bounce/blush timers as well as advanced boops.
 auto &state=animation::state();state.isEyeBouncing=true;state.blushState=animation::BlushState::Full;
 boopRevertAt=clockMs+3000;selectUserView(VIEW_CIRCLE_EYES);
 assert(!state.isEyeBouncing && state.blushState==animation::BlushState::Inactive && !boopRevertAt);

 // Offline and asleep collection uses the same one-second clock as the wire.
 clockMs=0;temperatureMillis=0;clearHistoryBuffer();deviceConnected=false;
 updateTemperature();assert(temperatureReads==0);
 for(int i=1;i<=3;++i){clockMs=i*1000;sleepModeActive=(i==3);updateTemperature();}
 assert(temperatureReads==3 && history.snapshot().count==3);
 assert(temperature.notifications.empty());

 // Reading a history snapshot must not change halfway through a transfer,
 // even when the sampling task appends while START is being notified.
 deviceConnected=true;
 historyCharacteristic.onNotify=[](){if(clockMs==3000){clockMs=4000;updateTemperature();}};
 triggerHistoryTransfer();
 assert(history.snapshot().count==4);
 const auto &packets=historyCharacteristic.notifications;
 assert(packets.size()==3 && packets[0][0]==0xA0 && packets[0][2]==1 && packets[0][4]==3);
 assert(packets[1][0]==0xA1 && packets[1][3]==3 && packets[2][0]==0xA2);

 // Gaps cannot be represented as contiguous one-second readings.
 clockMs=10000;updateTemperature();assert(history.snapshot().count==1);
 puts("Production sleep/button/configuration, offline sampling and history snapshot regressions passed.");
}
'''


class RuntimeRegressionTest(unittest.TestCase):
    def test_production_controllers(self):
        files = ["config/RuntimeSettings.cpp", "app/PowerManagement.cpp", "app/ViewState.cpp",
                 "app/InputController.cpp", "ble/ble_metrics.cpp"]
        parts = [STUB]
        for name in files:
            source = (ROOT / "src" / name).read_text(encoding="utf-8")
            parts.append("\n".join(line for line in source.splitlines() if not line.startswith("#include")))
        parts.append(TESTS)
        with tempfile.TemporaryDirectory(prefix="lumifur-runtime-") as directory:
            source, executable = Path(directory) / "test.cpp", Path(directory) / "test.exe"
            source.write_text("\n".join(parts), encoding="utf-8")
            subprocess.run(["g++", "-std=c++17", f"-I{ROOT / 'src'}", str(source),
                            str(ROOT / "src/core/TemperatureHistory.cpp"), str(ROOT / "src/core/AnimationState.cpp"),
                            "-o", str(executable)], check=True)
            subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    unittest.main()
