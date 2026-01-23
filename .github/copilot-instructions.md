# GitHub Copilot Instructions for LumiFur Controller

## Project Overview
LumiFur Controller is an embedded C++ project for ESP32 microcontrollers that controls LED matrix displays for Protogen masks. The system features facial expression rendering, smooth animations, and Bluetooth Low Energy (BLE) connectivity for remote control.

### Key Technologies
- **Platform**: ESP32 (Espressif32) with Arduino framework
- **Build System**: PlatformIO with multiple environments
- **Display**: HUB75 64x32 LED matrix panels (2x units)
- **Graphics**: Adafruit GFX library for rendering
- **Communication**: NimBLE for Bluetooth LE connectivity
- **Testing**: Unity and GoogleTest frameworks

## Architecture & Code Structure

### Main Components
- `main.cpp` (~1500 lines): Core application logic, view management, animations
- `main.h`: Function declarations, configuration constants, helper functions
- `bitmaps.h`: Bitmap data for facial expressions and graphics
- `ble.h`: Bluetooth Low Energy communication handling
- `deviceConfig.h`: Hardware pin definitions and device-specific settings
- `userPreferences.h`: User settings and preferences management

### Key Concepts
- **Views**: Different facial expressions (idle, happy, angry, playful, etc.)
- **Maws**: Mouth/jaw variations for expressions
- **Frames**: Animation frame management for smooth transitions
- **Brightness Control**: Adaptive brightness using APDS9960 sensor
- **BLE Commands**: Remote control via Bluetooth characteristics

## Coding Guidelines & Patterns

### Embedded C++ Best Practices
- **Memory Management**: Be mindful of ESP32's limited RAM (~320KB)
- **Stack Usage**: Prefer stack allocation over dynamic allocation
- **Const Correctness**: Use `const` for read-only data, especially large bitmaps
- **PROGMEM**: Store large static data (bitmaps, fonts) in flash memory using `PROGMEM`
- **Volatile Variables**: Use `volatile` for variables modified in interrupts

### Naming Conventions
- **Variables**: camelCase (e.g., `currentView`, `userBrightness`)
- **Functions**: camelCase (e.g., `debounceButton()`, `setupMatrix()`)
- **Constants**: UPPER_SNAKE_CASE (e.g., `PANEL_WIDTH`, `MAX_BRIGHTNESS`)
- **Classes**: PascalCase (rarely used in this embedded context)

### Code Organization Patterns
```cpp
// Hardware interaction functions
void setupMatrix();
void setupBLE();

// Animation and display functions  
void renderView(uint8_t viewNumber);
void updateAnimation();

// Utility functions
bool debounceButton(int pin);
float easeInOutQuad(float t);

// BLE callback handlers
void onBLECommand(uint8_t command);
```

### Error Handling
- Use return codes for functions that can fail
- Implement proper initialization checks for hardware
- Add debug output using `Serial.print()` when `DEBUG_MODE` is enabled
- Handle BLE connection states gracefully

## Hardware-Specific Considerations

### LED Matrix Management
- **DMA Usage**: The HUB75 library uses DMA for smooth refresh rates
- **Color Depth**: Work with 16-bit RGB565 color format
- **Brightness**: Linear brightness scaling can appear non-linear to human eye
- **Refresh Rate**: Maintain consistent frame rates for smooth animations

### Memory Constraints
- **Bitmap Storage**: Large bitmaps should use `PROGMEM` to save RAM
- **Buffer Management**: Be careful with large frame buffers
- **String Literals**: Use `F()` macro for string literals to save RAM

### Power Management
- **Sleep Modes**: Consider deep sleep for power conservation
- **Pin Management**: Properly configure unused pins
- **Current Draw**: LED matrices can draw significant current

## PlatformIO Environment Guidelines

### Available Environments
- `adafruit_matrixportal_esp32s3`: Primary target (default)
- `esp32dev`: Generic ESP32 development
- `native`: For unit testing
- `codeql`: Static analysis and testing

### Build Commands Context
```bash
# Build for default environment
pio run

# Build for specific environment
pio run -e adafruit_matrixportal_esp32s3

# Upload to device
pio run -t upload

# Run tests
pio test -e native

# Monitor serial output
pio device monitor
```

### Library Dependencies
All required libraries are defined in `platformio.ini`:
- Adafruit GFX Library (graphics primitives)
- ESP32 HUB75 LED MATRIX PANEL DMA Display (matrix control)
- NimBLE-Arduino (Bluetooth Low Energy)
- FastLED (color utilities)
- Adafruit sensor libraries (LIS3DH, APDS9960)

## Testing Guidelines

### Test Structure
- **Native Tests**: Use Unity framework for core logic testing
- **GoogleTest**: Available for more complex test scenarios  
- **Hardware Tests**: Test on actual hardware when possible
- **Coverage**: Code coverage analysis available via test environments

### Testing Patterns
```cpp
// Unity test example
void test_easing_function() {
    TEST_ASSERT_EQUAL_FLOAT(0.0f, easeInQuad(0.0f));
    TEST_ASSERT_EQUAL_FLOAT(1.0f, easeInQuad(1.0f));
}

// Mock hardware interactions for testing
void test_view_switching() {
    uint8_t initialView = currentView;
    // Test view switching logic
    TEST_ASSERT_NOT_EQUAL(initialView, currentView);
}
```

## BLE Communication Patterns

### Command Structure
- Single byte commands for simplicity
- Command validation and error handling
- State synchronization between device and client
- Connection state management

### Implementation Pattern
```cpp
// BLE callback structure
class MyBLECallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pCharacteristic) {
        uint8_t command = pCharacteristic->getValue()[0];
        handleBLECommand(command);
    }
};
```

## Animation & Graphics Guidelines

### Animation Principles
- **Smooth Transitions**: Use easing functions for natural motion
- **Frame Timing**: Maintain consistent frame rates
- **State Management**: Track animation states properly
- **Resource Cleanup**: Clear buffers between animations

### Graphics Optimization
- **Dirty Rectangle**: Only update changed regions when possible
- **Color Palette**: Use efficient color representations
- **Bitmap Compression**: Consider simple compression for large bitmaps
- **Double Buffering**: Use when smooth animations are critical

## Debugging & Monitoring

### Debug Output
- Use conditional compilation with `DEBUG_MODE`
- Structured debug output with categories (`DEBUG_BLE`, `DEBUG_VIEWS`)
- Serial output for real-time monitoring
- Performance profiling for critical paths

### Common Debug Patterns
```cpp
#if DEBUG_MODE
  Serial.print("Switching to view: ");
  Serial.println(newView);
#endif

#ifdef DEBUG_BLE
  Serial.print("BLE command received: 0x");
  Serial.println(command, HEX);
#endif
```

## Performance Considerations

### Critical Paths
- **Display Refresh**: Matrix updates should be non-blocking
- **Animation Updates**: Keep animation logic lightweight
- **BLE Handling**: Process BLE commands quickly
- **Sensor Reading**: Avoid blocking sensor operations

### Optimization Strategies
- **Function Inlining**: Use `inline` for frequently called functions
- **Loop Optimization**: Minimize work inside tight loops
- **Memory Access**: Prefer stack variables over globals
- **Compiler Flags**: Leverage PlatformIO optimization settings

## Integration with Existing Codebase

### When Adding Features
1. **Check Existing Patterns**: Follow established code organization
2. **Update Documentation**: Modify relevant README sections
3. **Add Tests**: Include unit tests for new functionality
4. **Consider Hardware**: Validate on actual ESP32 hardware
5. **Update Build**: Ensure all environments compile successfully

### Code Review Checklist
- [ ] Memory usage implications considered
- [ ] Hardware constraints respected
- [ ] Error handling implemented
- [ ] Debug output added appropriately
- [ ] Tests updated or added
- [ ] Documentation updated

## Contributing Workflow

### Before Code Changes
1. Review the comprehensive `CONTRIBUTING.md`
2. Set up PlatformIO development environment
3. Build and test current codebase
4. Create descriptive feature branch
5. Run existing test suite

### Development Process
1. Make incremental changes
2. Test frequently on hardware
3. Add appropriate debug output
4. Update tests as needed
5. Validate against all build environments

This guidance helps GitHub Copilot understand the embedded nature, hardware constraints, and specific patterns used in the LumiFur Controller project for more accurate and helpful code suggestions.

## Copilot Notes: Performance-First ESP32-S3 Workflow

You are GitHub Copilot operating inside an ESP32-S3 firmware repository that primarily uses the Arduino framework (with some ESP-IDF headers/features). The firmware is real-time and performance sensitive (e.g., HUB75/I2S DMA display, animation “views”, sensors, NimBLE BLE + OTA, NVS preferences). Your assignment is to optimise CPU time, frame pacing, heap stability, and memory locality WITHOUT changing externally observable behaviour.

================================================================================
0) NON-NEGOTIABLE REQUIREMENTS (DO NOT VIOLATE)
================================================================================
A. Behavioural invariants
- Preserve ALL external behaviour 1:1:
  - BLE UUIDs, services/characteristics, packet formats, command IDs, response formats
  - OTA update protocol/state transitions, chunking, ACK/NAK behaviour, timeouts
  - View IDs and view switching logic, brightness rules, gamma rules, sensor thresholds
  - Preferences keys, default values, migration logic (NVS/Preferences), saved state
  - Timing-visible behaviour (e.g., animation feel, easing curves, debouncing windows)

B. Real-time rules
- Never introduce blocking into the render/scanout critical path:
  - No delay(), vTaskDelay() or busy-waits in code that affects frame output timing
- Any heavy work triggered by callbacks must be deferred:
  - BLE callbacks, ISR handlers, I2C interrupts must enqueue work and return quickly

C. Optimisation must be measurable
- Add minimal instrumentation to prove the change:
  - frame time (min/avg/max), FPS, CPU time in hot loops, heap stats
- Instrumentation MUST be gated:
  - #ifdef PROFILE or #if PERF_DIAGNOSTICS
- Logs must be periodic summaries (e.g., once per second), never per frame.

D. Safety + reliability
- Do not create races. If you change concurrency, use queues/atomics and prove thread safety.
- Do not increase heap fragmentation or risk WDT resets.
- Avoid flash writes near time-critical loops (SPI flash writes stall normal tasks).

================================================================================
1) COPILOT WORKFLOW: HOW YOU MUST APPROACH ANY OPTIMISATION
================================================================================
Step 1 — Identify hot paths (always start here)
- Render pipeline:
  - per-frame update/render functions
  - pixel packing (RGB888->RGB565 / bitplane packing), blending, gamma
  - DMA buffer fill, scanline/row generation
- BLE + OTA:
  - characteristic write callbacks
  - packet parsing and validation
  - OTA chunk buffering/CRC
- Sensors / interrupts:
  - ISR stubs, any immediate follow-up work
  - polling loops that run frequently
- Logging and debugging paths that might run too often

Step 2 — Add profiling that does NOT perturb real-time behaviour
- Prefer esp_timer_get_time() (us) for medium-grain:
  - renderFrame(), packRow(), bleParsePacket()
- For micro-loops, use cycle counts if available and safe:
  - cpu_hal_get_cycle_count() (esp-idf) / esp_cpu_get_cycle_count()
- Collect stats into small structs; compute min/max/avg offline, print periodically.

Required profile outputs (minimum):
- Frame timing:
  - last_frame_us, min_frame_us, max_frame_us, avg_frame_us (rolling window)
- Heap health:
  - ESP.getFreeHeap()
  - heap_caps_get_largest_free_block(MALLOC_CAP_8BIT) (largest contiguous block)
- Optional:
  - task CPU use (if IDF hooks enabled)
  - render vs BLE vs sensor time slices

Step 3 — Optimise in descending impact order
1) Heap churn elimination (allocations in hot paths)
2) Memory tiering (internal RAM vs PSRAM) and locality
3) Algorithmic wins (math, LUTs, branch elimination, batching)
4) Tight loop mechanics (inlining, invariants, pointer caching)
5) RTOS scheduling/core placement and concurrency (only if needed)
6) Build/linker flags (only if project uses PlatformIO/IDF-style flags)

Step 4 — Validate after each change
- Compile and run (or preserve buildability)
- Confirm behaviour is identical:
  - same packets, same view transitions, same persisted keys
- Confirm performance:
  - improved or unchanged timing + heap metrics
- Keep diffs small and reviewable.

================================================================================
2) PERFORMANCE STRATEGY: WHAT “GOOD” LOOKS LIKE ON ESP32-S3
================================================================================
Primary objective is STABLE frame pacing (low jitter), not just high FPS.
- A stable 60 FPS feels better than 75 FPS with stutter.
- Most stutter comes from:
  - heap churn/fragmentation
  - flash cache misses / flash writes
  - logging/printf
  - PSRAM working-set access
  - BLE parsing done in callbacks
  - contention between tasks/cores

================================================================================
3) THE OPTIMISATION RULEBOOK (APPLY THESE AGGRESSIVELY)
================================================================================

--------------------------------------------------------------------------------
A) HEAP & ALLOCATION: THE BIGGEST EMBEDDED WIN
--------------------------------------------------------------------------------
A1. Absolutely no allocation in hot paths
Hot paths include:
- renderFrame(), draw/pack loops, DMA fill callbacks, per-row/per-scanline work
- BLE onWrite/onNotify callbacks, packet handlers
- ISR follow-ups if they run frequently

Forbidden in hot paths:
- String / std::string concatenation
- new/delete/malloc/free
- vector growth, map/unordered_map operations that allocate
- ArduinoJson dynamic allocations unless using StaticJsonDocument and reusing it

Replace with:
- fixed buffers + snprintf
- preallocated ring buffers
- StaticJsonDocument or manual parsing into structs

Example conversion:
BAD:
  String s = "Temp:" + String(t) + "C";
GOOD:
  char buf[32];
  snprintf(buf, sizeof(buf), "Temp:%dC", t);

A2. If you MUST use std::vector, reserve once and reuse
- reserve() in setup/init
- clear() does not free capacity, good for reuse
- never push_back without capacity in hot loops

A3. Avoid allocator-heavy containers
- Do not use std::map/unordered_map/list in firmware hot paths
- Prefer:
  - fixed arrays
  - small flat lookup tables (struct {key,val}[])
  - ring buffers (single-producer/single-consumer where possible)

A4. Use “largest free block” as fragmentation indicator
- Total free heap can look fine while the largest block collapses.
- Track:
  - free_heap
  - largest_free_block
- Optimise to keep largest_free_block stable over time.

--------------------------------------------------------------------------------
B) MEMORY TIERING: INTERNAL RAM VS PSRAM (ESP32-S3 CRITICAL)
--------------------------------------------------------------------------------
B1. Treat PSRAM as cold storage only
- Store:
  - large, rarely touched assets (sprites, cached frames)
  - long logs (if any) or large lookup tables that are not hot
- Do NOT keep working-set data (touched per frame) in PSRAM.

B2. Keep hot working sets in internal RAM
Hot working set examples:
- current framebuffer / scanline buffer
- per-frame state arrays
- palette/gamma tables used every pixel

B3. If data must live in PSRAM, stage it
Use chunked processing:
- Copy PSRAM -> internal RAM chunk
- Compute in internal RAM
- Copy results back if needed

Use triple-buffer idea:
- Chunk A: copy-out (async if available)
- Chunk B: compute
- Chunk C: copy-in (async)
Rotate A/B/C each iteration.

B4. Operate in wider words where safe
- Prefer 32-bit operations (uint32_t) over bytewise loops when alignment allows
- For pixel packing/copies, process 4 bytes per iteration to reduce loop overhead.

B5. Align buffers that benefit from DMA/vector ops
- Align to 4 or 16 bytes where appropriate
- For DMA buffers, allocate with MALLOC_CAP_DMA if required by driver.

--------------------------------------------------------------------------------
C) MATH & TABLES: MAKE INNER LOOPS CHEAP
--------------------------------------------------------------------------------
C1. Avoid float in per-pixel/per-frame hot loops
- Use fixed-point (scaled ints)
- Avoid pow/sin/cos/sqrt in render loops

C2. Precompute LUTs
- gamma[256]
- sin/cos tables for animation
- easing curves
- RGB conversion tables if frequently used (careful with memory size)

C3. Replace if-chains with LUTs/switch
- LUT for classification, thresholds, mapping, gamma
- switch for command IDs rather than many if/else

C4. Hoist invariants
- Anything constant per frame goes outside pixel loops:
  - brightness scale factors
  - pointer to current buffer
  - bounds, widths, row offsets

--------------------------------------------------------------------------------
D) TIGHT LOOP MECHANICS: SMALL CHANGES, BIG GAINS
--------------------------------------------------------------------------------
D1. Inline hot helpers
- static inline for clamp, pack, blend, gamma lookup
- avoid function pointer / virtual dispatch in inner loops

D2. Cache pointers and frequently used fields
Inside loops:
- auto* dst = buf;
- const int w = width;
- const int stride = STRIDE;
Avoid repeated struct member reads in tight loops.

D3. Reduce aliasing ambiguity
- Use __restrict__ on pointers where safe:
  void pack(const uint8_t* __restrict__ src, uint16_t* __restrict__ dst);

D4. Minimise branches
- Use branchless clamps where safe
- Precompute masks and do bitwise ops

D5. Avoid expensive generic Arduino helpers in hot loops
- Avoid map(), constrain() if it produces slow generic code in a tight loop
- Prefer explicit integer math.

--------------------------------------------------------------------------------
E) INTERRUPTS, CALLBACKS, AND CONCURRENCY
--------------------------------------------------------------------------------
E1. ISRs must be tiny and IRAM-safe
Rules:
- ISR does: set flag / push minimal item to queue, return
- No Serial, no BLE, no I2C, no malloc
- Mark ISR IRAM_ATTR
- Keep critical sections short.

E2. BLE callbacks must return fast
- Copy payload to a preallocated buffer or queue
- Parse/validate in a worker task (or in loop slow-path)
- Never do heavy JSON parsing, decompression, or display changes directly in callback.

E3. Split fast path and slow path
Fast path:
- rendering, DMA feeding, time-sensitive IO
Slow path:
- logs, BLE parsing, OTA CRC, preferences housekeeping

E4. Use FreeRTOS tasks intentionally (if present)
- Consider pinning:
  - render-critical work to one core
  - BLE/parse/sensors to the other
- Do not starve system tasks; respect Wi-Fi/BLE needs.

E5. Avoid flash writes during time-critical activity
- NVS writes stall and cause jitter.
- Batch preference writes and perform them on slow path (or on idle).

--------------------------------------------------------------------------------
F) FLASH/CACHE/IRAM: AVOID STALLS AND CACHE MISSES
--------------------------------------------------------------------------------
F1. Move truly hot functions to IRAM (strategic, not everywhere)
Candidates:
- scanline render
- pixel pack inner loops
- time-critical ISR helpers
Tradeoff:
- IRAM is limited; keep only what is hot.

F2. Avoid flash writes near render
- Flash writes suspend normal execution; can cause stutters or missed deadlines.

F3. Build options (if in scope)
- Use -O2 for speed builds, -Os for size builds
- Consider LTO
- Disable exceptions/RTTI if unused:
  -fno-exceptions -fno-rtti
Do NOT apply build flags without verifying compatibility.

--------------------------------------------------------------------------------
G) LOGGING DISCIPLINE (A COMMON FPS KILLER)
--------------------------------------------------------------------------------
G1. No logs in hot loops / ISRs / DMA callbacks
G2. Use compile-time log stripping
- #ifdef DEBUG / PROFILE
G3. Print periodic summaries only
- e.g., once per second:
  - FPS
  - avg/min/max frame time
  - free heap + largest free block
G4. If ESP-IDF logging is used, consider disabling dynamic log level control
(if you don’t need runtime changes) to reduce overhead.

================================================================================
4) REQUIRED OUTPUT FORMAT FOR ANY CHANGE YOU MAKE
================================================================================
When you implement an optimisation, you MUST:
- Add a short comment “WHY this is faster/stabler”
- Keep changes scoped and reviewable (no unrelated formatting)
- Wrap profiling in #ifdef PERF_DIAGNOSTICS
- Provide a before/after note in code comments if the change is non-obvious

Example:
  // PERF: avoid heap allocations by reusing a static buffer per frame.
  // PERF_DIAGNOSTICS: measures packRow() time in microseconds.

================================================================================
5) ACCEPTANCE CRITERIA (HOW TO KNOW YOU’RE DONE)
================================================================================
A change is acceptable only if:
- Behaviour remains identical (protocol, view IDs, prefs keys, outputs)
- Frame pacing jitter is reduced or unchanged
- Heap usage is stable over time (largest free block does not steadily shrink)
- BLE responsiveness is unchanged or improved
- No new races, WDT issues, or memory corruption risks

================================================================================
6) APPLY THESE INSTRUCTIONS NOW
================================================================================
For the file you are editing:
- Identify the hot path in that file
- Add minimal timing/heap instrumentation (gated)
- Apply A→G rules above, starting with heap churn and memory locality
- Keep diff small and prove improvement with metrics
