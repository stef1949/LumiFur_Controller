# Unity Tests for LumiFur Controller

## Overview
This directory contains comprehensive Unity framework tests for the LumiFur Controller firmware.

## Test Suites

### test_animation_state
Tests for the `AnimationState` module (`src/core/AnimationState.cpp`)
- **7 tests** covering initialization, reset, blink timings, blush state, eye bounce, maw state, and idle hover offsets

### test_scroll_state  
Tests for the `ScrollState` module (`src/core/ScrollState.cpp`)
- **6 tests** covering initialization, speed updates, timing reset, and boundary conditions

### test_easing_functions
Tests for easing functions (`src/main.h`)
- **7 tests** covering `easeInQuad`, `easeOutQuad`, and `easeInOutQuad` functions
- Includes boundary condition tests, mid-value tests, and monotonicity verification

### test_ble
Tests for BLE (Bluetooth Low Energy) module (`src/ble.h`)
- **13 tests** covering temperature history management, scroll speed clamping, and packet structure
- Tests circular buffer behavior, wraparound logic, and chunk calculation
- Validates temperature precision and history capacity

### Existing Tests
- `test_main/` - Main application tests
- `test_timers/` - Timer threshold tests
- `test_bitmaps/` - Bitmap validation tests
- `color_parser/` - Color parsing tests (GoogleTest)
- `test_mic/` - Microphone tests

## Running Tests

### Using PlatformIO
```bash
# Run all tests on native environment
pio test -e native

# Run all tests on native2 environment (with Unity)
pio test -e native2

# Run specific test
pio test -e native2 -f test_animation_state

# Run with verbose output
pio test -e native2 -vv
```

### Manual Compilation (for development/debugging)
If PlatformIO has issues, tests can be compiled manually using GCC:

```bash
# Clone Unity framework
git clone https://github.com/ThrowTheSwitch/Unity.git /tmp/unity-framework

# Compile AnimationState tests
g++ -std=c++11 \
  -I/tmp/unity-framework/src \
  -Isrc \
  -o test_animation_state \
  /tmp/unity-framework/src/unity.c \
  src/core/AnimationState.cpp \
  test/test_animation_state/test_animation_state.cpp \
  test_runner.cpp

# Run the test
./test_animation_state
```

## Test Results (Latest Run)

✅ **All 33 tests passing**

- AnimationState: 7/7 ✅
- ScrollState: 6/6 ✅  
- Easing Functions: 7/7 ✅
- BLE Module: 13/13 ✅

## Adding New Tests

1. Create a new test directory: `test/test_<module_name>/`
2. Create test file: `test_<module_name>.cpp`
3. Follow Unity framework conventions:
   - Include `<unity.h>`
   - Implement `setUp()` and `tearDown()` functions
   - Create test functions with `void test_*()` signature
   - Call tests in `setup()` using `RUN_TEST(test_name)`
   - Implement empty `loop()` function

Example test structure:
```cpp
#include <unity.h>
#include "module_to_test.h"

void setUp(void) {
  // Setup before each test
}

void tearDown(void) {
  // Cleanup after each test
}

void test_example(void) {
  TEST_ASSERT_EQUAL(1, 1);
}

void setup() {
  UNITY_BEGIN();
  RUN_TEST(test_example);
  UNITY_END();
}

void loop() {}
```

## Unity Assertions Reference

Common assertions used in these tests:
- `TEST_ASSERT_EQUAL(expected, actual)`
- `TEST_ASSERT_TRUE(condition)`
- `TEST_ASSERT_FALSE(condition)`
- `TEST_ASSERT_FLOAT_WITHIN(delta, expected, actual)`
- `TEST_ASSERT_GREATER_THAN_FLOAT(threshold, actual)` - actual > threshold
- `TEST_ASSERT_LESS_THAN_FLOAT(threshold, actual)` - actual < threshold

For more assertions, see: https://github.com/ThrowTheSwitch/Unity/blob/master/docs/UnityAssertionsReference.md
