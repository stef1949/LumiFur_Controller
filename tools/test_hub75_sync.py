"""Native checks for the DMA patch and its buffer ownership/brightness contract."""
import importlib.util
from pathlib import Path
import subprocess
import tempfile
import unittest
import runpy
from types import SimpleNamespace

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("hub75_patch", ROOT / "tools/platformio/prepare_hub75_driver.py")
patch = importlib.util.module_from_spec(spec)
spec.loader.exec_module(patch)

PANEL_FIXTURE = """
class Panel {
public:
    void setBrightness(const uint8_t b) {
        brightness = b;
        setBrightnessOE(b, 0);
        setBrightnessOE(b, 1);
    }
    int brightness = 128;
    int back_buffer_id = 1;
    bool initialized = true;
    int writes[2] = {0,0};
    int values[2] = {-1,-1};
    void setBrightnessOE(int b,int id) { ++writes[id]; values[id]=b; }
};
"""

NATIVE_BUS = r'''
#include <cstdint>
#include <cassert>
#include <cstdio>
#define IRAM_ATTR
constexpr int pdFALSE=0,pdTRUE=1,portMAX_DELAY=-1;
using BaseType_t=int;
using gdma_channel_handle_t=void*;
struct gdma_event_data_t { intptr_t tx_eof_desc_addr; };
struct dma_descriptor_t { dma_descriptor_t *next=nullptr; };
struct Semaphore { int tokens=0; };
using SemaphoreHandle_t=Semaphore*;
using StaticSemaphore_t=Semaphore;
int lockDepth=0;
#define portENTER_CRITICAL(p) (++lockDepth)
#define portEXIT_CRITICAL(p) (--lockDepth)
#define portENTER_CRITICAL_ISR(p) (++lockDepth)
#define portEXIT_CRITICAL_ISR(p) (--lockDepth)
void driveDma();
int waits=0;
int xSemaphoreTake(SemaphoreHandle_t s,int timeout) {
  assert(s);
  if(timeout==portMAX_DELAY) {
    assert(lockDepth==0); ++waits; driveDma();
  }
  if(!s->tokens) return pdFALSE;
  --s->tokens; return pdTRUE;
}
void xSemaphoreGiveFromISR(SemaphoreHandle_t s,BaseType_t *wake) { ++s->tokens; *wake=pdTRUE; }
struct Bus_Parallel16 {
  dma_descriptor_t _dmadesc_a[2],_dmadesc_b[2];
  unsigned _dmadesc_count=2;
  bool _double_dma_buffer=true;
  int lfSwapMux=0;
  intptr_t lfExpectedEof=0;
  SemaphoreHandle_t lfSwapDone=nullptr;
  static bool lfOnFrameEof(gdma_channel_handle_t,gdma_event_data_t*,void*);
  void flip_dma_output_buffer(int);
};
Bus_Parallel16 *currentBus;
int targetId=1;
void driveDma() {
  auto *target=targetId ? currentBus->_dmadesc_b : currentBus->_dmadesc_a;
  auto *old=targetId ? currentBus->_dmadesc_a : currentBus->_dmadesc_b;
  assert(old[1].next==target && target[1].next==target);
  gdma_event_data_t stale{reinterpret_cast<intptr_t>(&old[0])};
  assert(!Bus_Parallel16::lfOnFrameEof(nullptr,&stale,currentBus));
  assert(currentBus->lfExpectedEof!=0 && currentBus->lfSwapDone->tokens==0);
  gdma_event_data_t done{reinterpret_cast<intptr_t>(&target[0])};
  assert(Bus_Parallel16::lfOnFrameEof(nullptr,&done,currentBus));
  assert(currentBus->lfExpectedEof==0);
}
'''


class Hub75SyncTests(unittest.TestCase):
    def test_panel_patch_is_idempotent(self):
        result = patch.patch_panel(PANEL_FIXTURE)
        self.assertEqual(result, patch.patch_panel(result))

    def test_unknown_layout_fails_closed(self):
        with self.assertRaises(RuntimeError):
            patch.patch_panel("class UnexpectedPanel {};")

    def test_native_swap_and_brightness(self):
        code = NATIVE_BUS + patch.BUS_SWAP + patch.patch_panel(PANEL_FIXTURE) + r'''
int main() {
  Bus_Parallel16 bus; currentBus=&bus;
  // Library begin() calls flip before DMA starts: no wait on a null semaphore.
  bus.flip_dma_output_buffer(0); assert(waits==0 && bus.lfExpectedEof==0);
  Semaphore sem; bus.lfSwapDone=&sem;
  sem.tokens=1; // An old notification must not release the next swap early.
  bus.flip_dma_output_buffer(1); assert(waits==1 && sem.tokens==0);
  targetId=0; bus.flip_dma_output_buffer(0); assert(waits==2);
  Panel panel;
  panel.setBackBufferBrightness(100);
  assert(panel.writes[0]==0 && panel.writes[1]==1);
  panel.setBackBufferBrightness(100); assert(panel.writes[1]==1);
  panel.back_buffer_id=0; panel.setBackBufferBrightness(100);
  assert(panel.writes[0]==1 && panel.values[0]==100 && panel.values[1]==100);
  panel.back_buffer_id=1; panel.setBackBufferBrightness(30);
  assert(panel.values[0]==100 && panel.values[1]==30);
  panel.setBrightness(80); // Legacy initialization invalidates per-buffer caches.
  panel.setBackBufferBrightness(30); assert(panel.values[1]==30);
  puts("DMA ownership, stale EOF rejection, startup swap and back-buffer brightness passed.");
}
'''
        (ROOT / ".pio").mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(dir=ROOT / ".pio") as folder:
            source = Path(folder) / "sync.cpp"
            executable = Path(folder) / "sync.exe"
            source.write_text(code)
            subprocess.run(["g++", "-std=c++11", str(source), "-o", str(executable)], check=True)
            subprocess.run([str(executable)], check=True)

    def test_installed_driver_layout(self):
        # Optional integration check against the actual installed dependency.
        dependencies = ROOT / ".pio/libdeps/adafruit_matrixportal_esp32s3"
        matches = list(dependencies.glob("*/src/ESP32-HUB75-MatrixPanel-I2S-DMA.h"))
        if not matches:
            self.skipTest("HUB75 dependency not installed")
        root = matches[0].parents[1]
        for path, transform in (
            (matches[0], patch.patch_panel),
            (root / "src/platforms/esp32s3/gdma_lcd_parallel16.hpp", patch.patch_bus_header),
            (root / "src/platforms/esp32s3/gdma_lcd_parallel16.cpp", patch.patch_bus_source),
        ):
            result = transform(path.read_text())
            self.assertEqual(result, transform(result))
            if path.suffix == ".cpp":
                self.assertIn("current_idx == 0 || is_last", result)
                self.assertIn("const bool running = lfSwapDone != nullptr;", result)
                self.assertIn("reinterpret_cast<intptr_t>(&target[0])", result)

    def test_platformio_hook_patches_fresh_dependency(self):
        dependencies = ROOT / ".pio/libdeps/adafruit_matrixportal_esp32s3"
        matches = list(dependencies.glob("*/src/ESP32-HUB75-MatrixPanel-I2S-DMA.h"))
        if not matches:
            self.skipTest("HUB75 dependency not installed")
        driver = matches[0].parents[1]
        with tempfile.TemporaryDirectory(dir=ROOT / ".pio") as folder:
            target = Path(folder) / "test/Driver"
            relative_paths = ["src/ESP32-HUB75-MatrixPanel-I2S-DMA.h",
                              "src/platforms/esp32s3/gdma_lcd_parallel16.hpp",
                              "src/platforms/esp32s3/gdma_lcd_parallel16.cpp"]
            for relative in relative_paths:
                path = target / relative
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text((driver / relative).read_text())

            class Environment:
                def IsCleanTarget(self):
                    return False

                def PioPlatform(self):
                    return SimpleNamespace(name="espressif32")

                def subst(self, name):
                    return {"$PROJECT_LIBDEPS_DIR": folder, "$PIOENV": "test"}[name]

            runpy.run_path(str(ROOT / "tools/platformio/prepare_hub75_driver.py"),
                           init_globals={"Import": lambda name: None, "env": Environment()})
            for relative in relative_paths:
                self.assertIn(patch.MARKER, (target / relative).read_text())


if __name__ == "__main__":
    unittest.main()
