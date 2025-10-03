<p align="center">
<img width="300" alt="LumiFur Controller" src="docs/mps3.png">
</p>
<h1 align="center">
  LumiFur Controller 
  
[![CodeQL Advanced Build with PlatformIO](https://github.com/stef1949/LumiFur_Controller/actions/workflows/codeql.yml/badge.svg?branch=main)](https://github.com/stef1949/LumiFur_Controller/actions/workflows/codeql.yml)
[![Build Firmware Artifacts](https://github.com/stef1949/LumiFur_Controller/actions/workflows/build-firmware.yml/badge.svg?branch=main)](https://github.com/stef1949/LumiFur_Controller/actions/workflows/build-firmware.yml)
  ![version](https://img.shields.io/badge/version-0.2.0-blue)
  <a href="https://github.com/badges/shields/pulse" alt="Activity">
        <img src="https://img.shields.io/github/commit-activity/m/badges/shields" /></a>
  [![Coverage Status](https://coveralls.io/repos/github/stef1949/LumiFur_Controller/badge.svg?branch=main)](https://coveralls.io/github/stef1949/LumiFur_Controller?branch=main)
  
  


</h1>
A program for controlling an LED matrix display for a Protogen mask, featuring various facial expressions and Bluetooth LE control with an ESP32.

## Table of Contents

- ✨ [Features](#features)
- 🛠️ [Hardware Requirements](#hardware-requirements)
- 💻 [Software Requirements](#software-requirements)
- ⚙️ [Installation](#installation)
- 📖 [Usage](#usage)
- 😃 [Facial Expressions](#facial-expressions)
- 🤖 [GitHub Copilot Integration](#github-copilot-integration)
- 🤝 [Contributing](#contributing)
- 📜 [License](#license)

## ✨ Features
- Multiple facial expressions (idle, happy, angry, playful, silly, lewd, and more)
- Smooth blinking animations
- Bluetooth Low Energy (BLE) connectivity to switch expressions remotely
- Boot-up animation with scrolling text
  
## 🛠️ Hardware Requirements
- ESP32 development board
- 2 x Hub75 64x32 LED matrix displays
- Connecting wires and power supply
  
## 💻 Software Requirements
- VSCode with PlatformIO Extension 
- MatrixPanel-DMA library
- FastLED Library
- NimBLE-Arduino library
- Adafruit GFX Library
- Adafruit NeoPixel Library

## ⚙ Installation

1. Clone the repository:
    ```sh
    git clone https://github.com/stef1949/LumiFur_Controller.git
    ```

2. Install the required libraries in Arduino IDE:

### MD_MAX72XX Library:

- Go to Sketch > Include Library > Manage Libraries...
- Search for MD_MAX72XX and install it.

### NimBLE-Arduino Library:

- Follow the installation instructions here.

3. Open the project:

- Open main.cpp in the Arduino IDE.

4. Configure hardware settings if necessary:

- Check the pin definitions (CLK_PIN, DATA_PIN, CS_PIN) and adjust them to match your setup.

## 🔧 Firmware Builds & Releases

The LumiFur Controller uses an automated build system to generate firmware artifacts for different ESP32 hardware configurations.

### 📦 Pre-built Firmware

Pre-built firmware artifacts are automatically generated for each release and can be downloaded from the [Releases](https://github.com/stef1949/LumiFur_Controller/releases) page. Each release includes:

- **firmware.bin** - Main application firmware for OTA (Over-The-Air) updates
- **partitions.bin** - Partition table required for OTA functionality  
- **bootloader.bin** - ESP32 bootloader binary
- **firmware.elf** - Debug symbols (when available)
- **build-info.txt** - Build metadata and version information

### 🎯 Supported Hardware

Firmware is built for multiple ESP32 environments:

- **`adafruit_matrixportal_esp32s3`** - Primary target (Adafruit MatrixPortal ESP32-S3)
- **`esp32dev`** - Generic ESP32 development boards  
- **`dev`** - Development build with extra instrumentation

### 🚀 Installing Pre-built Firmware

#### Option 1: OTA Update (Recommended)
If you already have LumiFur Controller running:
1. Use the companion app or BLE interface to initiate OTA update
2. Upload the `firmware.bin` file from your desired environment

#### Option 2: Complete Flash
For first-time installation or complete reflashing:
1. Use ESP32 flash tools (esptool.py, ESP32 Flash Download Tools, or PlatformIO)
2. Flash all three files in the correct order and memory locations:
   - `bootloader.bin` at 0x1000
   - `partitions.bin` at 0x8000  
   - `firmware.bin` at 0x10000

#### Option 3: PlatformIO Development
For development and customization:
```sh
# Clone the repository
git clone https://github.com/stef1949/LumiFur_Controller.git
cd LumiFur_Controller

# Build for your target environment
pio run -e adafruit_matrixportal_esp32s3

# Flash over USB
pio run -e adafruit_matrixportal_esp32s3 --target upload

# Monitor serial output
pio device monitor -b 115200
```

### 🔄 Automatic Builds

The firmware build system automatically:
- ✅ Builds on every push to main branch
- ✅ Creates artifacts for pull requests  
- ✅ Generates release bundles for tagged versions
- ✅ Attaches firmware files to GitHub releases
- ✅ Includes build metadata and version information

## 📖 Usage

1. Connect your ESP32 to the computer via USB.
2. Select the correct board and port in the Arduino IDE.
3. Click Upload to flash the code.

### Assemble the hardware:

- Connect the LED matrix modules to the ESP32 according to your wiring configuration.

### Operate the Protogen mask:

1. Power on the device.
2. The mask will display the default idle face with animations.
3. Use the LumiFur app to connect to 'LumiFur_Controller'.
4. Send values between 1 and 8 to change facial expressions.

### 😃 Facial Expressions

- 1 - Idle Face
- 2 - Happy Face
- 3 - Angry Face
- 4 - Playful Face
- 5 - Silly Face
- 6 - Kinky Face
- 7 - Death Face
- 8 - Edgy Face

## 🤖 GitHub Copilot Integration

This project includes comprehensive GitHub Copilot instructions to help you develop more efficiently. The instructions provide context about:

- **Embedded C++ patterns** specific to ESP32 development
- **Hardware constraints** and memory management guidelines  
- **PlatformIO build system** usage and environment configurations
- **LED matrix and BLE** communication patterns
- **Animation and graphics** optimization techniques
- **Testing frameworks** (Unity and GoogleTest) integration

### Getting Started with Copilot

1. **Install Extensions**: The recommended VS Code extensions include GitHub Copilot
2. **Review Instructions**: Check `.github/copilot-instructions.md` for detailed guidance
3. **Use Chat Instructions**: Use `.github/copilot-chat-instructions.md` for Copilot Chat sessions

### Copilot-Assisted Development Tips

- **Hardware Context**: Copilot understands ESP32 memory constraints and embedded best practices
- **Code Patterns**: Follows established patterns for view management, animations, and BLE communication
- **Testing Support**: Generates appropriate Unity and GoogleTest test cases
- **Documentation**: Helps maintain consistent code documentation and comments

The Copilot instructions are designed to help both new contributors and experienced developers work more effectively with this embedded codebase.

## 🤝 Contributing
Contributions are welcome! Please fork the repository and submit a pull request with your improvements.

## 📜 License 
This project is licensed under the BSD 3-Clause License - see the LICENSE file for details.
