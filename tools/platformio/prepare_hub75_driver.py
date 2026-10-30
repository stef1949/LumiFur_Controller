"""Project-owned HUB75 fixes, reapplied after dependency installation on every build.

Keep these changes outside .pio so a clean dependency install retains them. Unknown
driver layouts fail the build instead of silently dropping DMA synchronisation.
"""
from pathlib import Path

MARKER = "// LumiFur HUB75 synchronisation v2"
LEGACY_MARKER = "// LumiFur HUB75 synchronisation v1"


def replace_once(source, old, new):
    if source.count(old) != 1:
        raise RuntimeError("Unsupported HUB75 driver layout; cannot safely apply: " + old[:70])
    return source.replace(old, new, 1)


def patch_panel(source):
    if MARKER in source:
        return source
    if LEGACY_MARKER in source:
        return source.replace(LEGACY_MARKER, MARKER)
    source = replace_once(source, "    void setBrightness(const uint8_t b) {", """    // LumiFur HUB75 synchronisation v2
    // The renderer calls this before a swap, after the preceding swap has
    // acknowledged DMA completion. Never rewrite OE in the live buffer.
    void setBackBufferBrightness(const uint8_t b) {
        if (!initialized) return;
        brightness = b;
        if (lfBufferBrightness[back_buffer_id] != b) {
            setBrightnessOE(b, back_buffer_id);
            lfBufferBrightness[back_buffer_id] = b;
        }
    }

    void setBrightness(const uint8_t b) {""")
    source = replace_once(source, "        setBrightnessOE(b, 0);", """        lfBufferBrightness[0] = lfBufferBrightness[1] = -1;
        setBrightnessOE(b, 0);""")
    source = replace_once(source, "    int brightness = 128;", "    int lfBufferBrightness[2] = {-1, -1};\n    int brightness = 128;")
    return source


def patch_bus_header(source):
    if MARKER in source:
        return source
    if LEGACY_MARKER in source:
        return source.replace(LEGACY_MARKER, MARKER)
    source = replace_once(source, "#include <esp_private/gdma.h>", """#include <esp_private/gdma.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <esp_attr.h>""")
    source = replace_once(source, "    gdma_channel_handle_t dma_chan;", """    gdma_channel_handle_t dma_chan;
    // LumiFur HUB75 synchronisation v2
    StaticSemaphore_t lfSwapStorage;
    SemaphoreHandle_t lfSwapDone = nullptr;
    portMUX_TYPE lfSwapMux = portMUX_INITIALIZER_UNLOCKED;
    volatile intptr_t lfExpectedEof = 0;
    static bool IRAM_ATTR lfOnFrameEof(gdma_channel_handle_t channel,
                                      gdma_event_data_t *event, void *context);""")
    return source


def patch_bus_source(source):
    if MARKER in source:
        return source
    # A first-descriptor EOF proves entry into the new buffer without waiting
    # for an additional complete scan. Keep the existing frame EOF as well.
    source = replace_once(source, "desc->dw0.suc_eof = is_last ? 1 : 0;",
                          "desc->dw0.suc_eof = (current_idx == 0 || is_last) ? 1 : 0;")
    legacy = LEGACY_MARKER in source
    if not legacy:
        source = replace_once(source, "    gdma_start(dma_chan, (intptr_t)&_dmadesc_a[0]);", """    // Register before starting DMA; EOFs identify the first/last descriptor.
    if (lfSwapDone == nullptr) {
      lfSwapDone = xSemaphoreCreateBinaryStatic(&lfSwapStorage);
    }
    gdma_tx_event_callbacks_t callbacks = {};
    callbacks.on_trans_eof = lfOnFrameEof;
    ESP_ERROR_CHECK(gdma_register_tx_event_callbacks(dma_chan, &callbacks, this));
    gdma_start(dma_chan, (intptr_t)&_dmadesc_a[0]);""")
    signature = "  void Bus_Parallel16::flip_dma_output_buffer(int back_buffer_id)"
    start = source.index(signature)
    brace = source.index("{", start)
    depth, end = 1, brace + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    if legacy:
        start = source.rfind("\n", 0, source.index(LEGACY_MARKER)) + 1
    source = source[:start] + BUS_SWAP + source[end:]
    return source


BUS_SWAP = r'''  // LumiFur HUB75 synchronisation v2
  bool IRAM_ATTR Bus_Parallel16::lfOnFrameEof(gdma_channel_handle_t,
                                             gdma_event_data_t *event, void *context)
  {
    auto *bus = static_cast<Bus_Parallel16 *>(context);
    BaseType_t wake = pdFALSE;
    bool completed = false;
    portENTER_CRITICAL_ISR(&bus->lfSwapMux);
    if (bus->lfExpectedEof != 0 && bus->lfExpectedEof == event->tx_eof_desc_addr) {
      bus->lfExpectedEof = 0;
      completed = true;
    }
    portEXIT_CRITICAL_ISR(&bus->lfSwapMux);
    if (completed) xSemaphoreGiveFromISR(bus->lfSwapDone, &wake);
    return wake == pdTRUE;
  }

  void Bus_Parallel16::flip_dma_output_buffer(int back_buffer_id)
  {
    if (!_double_dma_buffer || _dmadesc_count == 0) return;
    // begin() primes the descriptor links before starting DMA. That first
    // swap must not wait for an interrupt from hardware that is not running.
    const bool running = lfSwapDone != nullptr;
    if (running) xSemaphoreTake(lfSwapDone, 0);
    portENTER_CRITICAL(&lfSwapMux);
    auto *target = back_buffer_id == 1 ? _dmadesc_b : _dmadesc_a;
    target[_dmadesc_count - 1].next = (dma_descriptor_t *)&target[0];
    auto *previous = back_buffer_id == 1 ? _dmadesc_a : _dmadesc_b;
    previous[_dmadesc_count - 1].next = (dma_descriptor_t *)&target[0];
    // Completion of the target's first descriptor proves DMA has entered the
    // new loop and released the old one. The previous EOF could be stale.
    lfExpectedEof = running ? reinterpret_cast<intptr_t>(&target[0]) : 0;
    portEXIT_CRITICAL(&lfSwapMux);
    // Sleep rather than busy-wait; DMA and both CPU cores continue running.
    // Do not hand the old buffer to the renderer without an acknowledgement.
    if (running) xSemaphoreTake(lfSwapDone, portMAX_DELAY);
  }'''


def patch_driver(root):
    root = Path(root)
    panel = root / "src/ESP32-HUB75-MatrixPanel-I2S-DMA.h"
    bus = root / "src/platforms/esp32s3/gdma_lcd_parallel16"
    edits = [(panel, patch_panel), (bus.with_suffix(".hpp"), patch_bus_header),
             (bus.with_suffix(".cpp"), patch_bus_source)]
    # Validate every edit before changing any file.
    prepared = [(path, transform(path.read_text())) for path, transform in edits]
    for path, updated in prepared:
        if path.read_text() != updated:
            path.write_text(updated)


if "Import" in globals():
    Import("env")
    if not env.IsCleanTarget() and env.PioPlatform().name == "espressif32":
        dependencies = Path(env.subst("$PROJECT_LIBDEPS_DIR")) / env.subst("$PIOENV")
        matches = list(dependencies.glob("*/src/ESP32-HUB75-MatrixPanel-I2S-DMA.h"))
        if len(matches) != 1:
            raise RuntimeError("Expected one HUB75 DMA dependency in " + str(dependencies))
        patch_driver(matches[0].parents[1])
