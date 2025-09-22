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