<p align="center">
<img width="300" alt="LumiFur Controller" src="docs/mps3.png">
</p>
<h1 align="center">
  LumiFur Controller<br>
  
[![CodeQL Advanced Build with PlatformIO](https://github.com/stef1949/LumiFur_Controller/actions/workflows/codeql.yml/badge.svg?branch=main)](https://github.com/stef1949/LumiFur_Controller/actions/workflows/codeql.yml)
  ![version](https://img.shields.io/badge/version-0.2.0-blue)
  <a href="https://github.com/badges/shields/pulse" alt="Activity">
        <img src="https://img.shields.io/github/commit-activity/m/badges/shields" /></a>
  [![Coverage Status](https://coveralls.io/repos/github/stef1949/LumiFur_Controller/badge.svg?branch=main)](https://coveralls.io/github/stef1949/LumiFur_Controller?branch=main)
</h1>
A real-time firmware for animating a HUB75 LED matrix Protogen mask with sensor-driven interactions, Bluetooth LE control, and OTA updates on the ESP32-based MatrixPortal platform.

## Table of Contents

- ✨ [Features](#features)
- ⚙️ [Hardware](#hardware)
- 🛠️ [Build & Flash](#build--flash)
- 🎛️ [Configuration](#configuration)
- 📡 [BLE Control](#ble-control)
- 🧪 [Testing](#testing)
- 🤖 [GitHub Copilot Integration](#github-copilot-integration)
- 🤝 [Contributing](#contributing)
- 📜 [License](#license)
- 🌐 [Web Firmware Updater](#web-firmware-updater)

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

## Contributing
Contributions are welcome! Please fork the repository, open an issue or discussion when appropriate, and submit a pull request following the guidelines in `CONTRIBUTING.md`.

## License
This project is licensed under the BSD 3-Clause License. See the `LICENSE` file for full text.
