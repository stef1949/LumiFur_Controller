<p align="center">
<img width="300" alt="LumiFur Controller" src="docs/mps3.png">
</p>
<h1 align="center">
  LumiFur Controller<br>
  
  [![CodeQL Advanced Build with PlatformIO](https://github.com/stef1949/LumiFur_Controller/actions/workflows/codeql.yml/badge.svg?branch=main)](https://github.com/stef1949/LumiFur_Controller/actions/workflows/codeql.yml)
  <img alt="GitHub Release" src="https://img.shields.io/github/v/release/stef1949/lumifur">
  <a href="https://github.com/stef1949/LumiFur_Controller" alt="Activity">
        <img src="https://img.shields.io/github/commit-activity/m/stef1949/LumiFur_Controller" /></a>
  [![Coverage Status](https://coveralls.io/repos/github/stef1949/LumiFur_Controller/badge.svg?branch=dev)](https://coveralls.io/github/stef1949/LumiFur_Controller?)
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

## Features
- 20+ animated faces and effects including plasma, flame, fluid, circle eyes, spiral overlays, starfields, and scrolling text purpose-built for dual 64×32 HUB75 panels.
- Sensor-driven reactions: the APDS9960 manages adaptive brightness and proximity-triggered blush/eye bounce, the LIS3DH accelerometer powers shake gestures and PixelDust physics, and an I2S MEMS microphone drives audio-reactive mouth animation.
- Bluetooth Low Energy control via NimBLE with remote expression switching, brightness management, sensor feature toggles, temperature telemetry, and a command channel for log retrieval.
- Wireless firmware delivery through a dedicated OTA characteristic plus runtime build metadata advertisement for companion apps.
- Power-aware runtime with wake-on-motion, sleep dimming, ambient light scaling, and persistent user preferences stored in ESP32 NVS.
- Optimized HUB75 pipeline using DMA double buffering, optional virtual panel simulation, and extensible C++ effect modules under `src/customEffects/`.

## Hardware
- **Controller:** Adafruit MatrixPortal ESP32-S3 (default target) with built-in APDS9960, LIS3DH, status NeoPixel, and onboard microphone used by the firmware.
- **Display:** Two chained 64×32 HUB75 RGB matrix panels (1/16 scan) plus appropriate ribbon cables and 5 V power delivery.
- **Inputs & sensors:** Onboard buttons map to view navigation; alternate hardware can be remapped in `src/deviceConfig.h`.
- **Optional:** Additional peripherals (e.g., alternative sensors or external microphones) can be integrated by adjusting the pin definitions and initialization blocks in `deviceConfig.h` / `main.cpp`.

## Build & Flash
All dependencies are resolved by PlatformIO. You can work from the CLI or through the VS Code extension.

```sh
git clone https://github.com/stef1949/LumiFur_Controller.git
cd LumiFur_Controller

# Build the default MatrixPortal ESP32-S3 environment
pio run -e adafruit_matrixportal_esp32s3

# Flash over USB
pio run -e adafruit_matrixportal_esp32s3 --target upload

# Optional: open the serial monitor at 115200 baud
pio device monitor -b 115200
```

Additional PlatformIO environments are defined in `platformio.ini`, including a `dev` flavor with extra instrumentation and native test environments (`codeql`, `native`, `native2`).

## Configuration
- **Persistent preferences:** `src/userPreferences.h` manages stored defaults for brightness, auto-blink, accelerometer usage, sleep mode, and other controller behaviors.
- **Hardware mapping:** Modify `src/deviceConfig.h` to adjust HUB75 pinouts, panel chains, button inputs, or to enable the virtual panel simulator (`VIRTUAL_PANE`).
- **Effects & assets:** Custom scenes live in `src/customEffects/` and bitmap assets in `src/bitmaps.h`; add new animations or alter existing ones there.
- **Build metadata:** `platformio.ini` sets `FIRMWARE_VERSION`, device model tags, and compiler flags shared across environments.

## BLE Control
- The controller advertises as `LumiFur_Controller` and exposes a GATT service (`01931c44-3867-7740-9867-c822cb7df308`).
- **Face characteristic:** write a view ID (0…`TOTAL_VIEWS`‑1) to switch expressions; read returns the current value.
- **Config characteristic:** four boolean flags toggle auto-brightness, accelerometer features, sleep mode, and aurora overlays.
- **Brightness characteristic:** write 0–255 to set manual brightness or subscribe for updates when auto-brightness adjusts.
- **Temperature & logs:** subscribe for live temperature readings and request buffered history through the command characteristic (0x01 to stream, 0x02 to clear).
- **OTA characteristic:** supports start/data packets for BLE firmware updates with status acknowledgements; pairing is recommended for production use.
- A JSON metadata characteristic exposes firmware version, git commit, build timestamp, and device ID for companion applications.
- The onboard NeoPixel pulses blue while advertising and turns green when a BLE client connects.

## Testing

[![Coverage Status](https://coveralls.io/repos/github/stef1949/LumiFur_Controller/badge.svg?branch=main)](https://coveralls.io/github/stef1949/LumiFur_Controller?branch=main)

- **77 Unity tests** covering core functionality: Run with `pio test -e native2`
- **Code coverage** tracked via [Coveralls](https://coveralls.io/github/stef1949/LumiFur_Controller) - see [docs/COVERAGE.md](docs/COVERAGE.md) for details
- Run the GoogleTest suite with coverage via `pio test -e codeql`
- Coverage reports and additional tooling scripts are located under `test/`
- Lightweight smoke tests for the Web Bluetooth firmware updater can run without PlatformIO: `python -m unittest discover docs/firmware-updater/tests`

## Web Firmware Updater
- A browser-based OTA helper lives at `docs/firmware-updater/index.html`. Serve the folder over HTTPS or `http://localhost` (for example, `python -m http.server 8000` from the repo root) because Web Bluetooth is blocked on `file://` origins. Open the page in a supported browser (Chrome or Edge), click **Connect** to choose your LumiFur controller, select a compiled `.bin` firmware file, and press **Upload Firmware** to stream it over the OTA characteristic (`01931c44-3867-7427-96ab-8d7ac0ae09ee`).
- Production builds are published automatically to GitHub Pages at [https://stef1949.github.io/LumiFur_Controller/](https://stef1949.github.io/LumiFur_Controller/); the root URL redirects to the `firmware-updater/` UI.
- Keep the page open during transfer; the device will reboot automatically after the update finishes.

## GitHub Copilot Integration
Developer onboarding guides for GitHub Copilot live in `docs/COPILOT_SETUP.md` and `docs/COPILOT_USAGE.md`, with tailored instructions for embedded patterns, animation workflows, and testing expectations.

## Contributing
Contributions are welcome! Please fork the repository, open an issue or discussion when appropriate, and submit a pull request following the guidelines in `CONTRIBUTING.md`.

## License
This project is licensed under the BSD 3-Clause License. See the `LICENSE` file for full text.
