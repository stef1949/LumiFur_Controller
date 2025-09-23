# Copilot Chat Instructions for LumiFur Controller

You are working on LumiFur Controller, an embedded C++ project for ESP32 microcontrollers that controls LED matrix displays for Protogen masks.

## Project Context
- **Hardware**: ESP32-S3, 2x HUB75 64x32 LED matrices, BLE connectivity  
- **Framework**: Arduino with PlatformIO build system
- **Purpose**: Animated facial expressions for Protogen fursuit masks
- **Memory**: Limited RAM (~320KB), use PROGMEM for large static data

## Key Files
- `main.cpp`: Core logic, view management, animations (~1500 lines)
- `bitmaps.h`: Facial expression bitmap data
- `ble.h`: Bluetooth Low Energy communication  
- `deviceConfig.h`: Hardware pin definitions
- `platformio.ini`: Build configurations and dependencies

## Code Style
- **Variables**: camelCase (`currentView`, `userBrightness`)
- **Functions**: camelCase (`renderView()`, `setupMatrix()`)  
- **Constants**: UPPER_SNAKE_CASE (`PANEL_WIDTH`, `MAX_BRIGHTNESS`)
- **Memory**: Use `const PROGMEM` for large bitmaps
- **Debug**: Conditional compilation with `#if DEBUG_MODE`

## Critical Patterns
- **Views**: Numbered facial expressions (0-10)
- **Animations**: Smooth transitions using easing functions
- **BLE Commands**: Single-byte commands for remote control
- **Error Handling**: Return codes, initialization checks
- **Performance**: Non-blocking matrix updates, lightweight animations

## Hardware Constraints
- Limited RAM - avoid dynamic allocation
- DMA-based matrix refresh - don't block
- Current draw considerations for LEDs
- Real-time requirements for smooth animations

When suggesting code, prioritize embedded best practices, memory efficiency, and hardware-appropriate patterns.