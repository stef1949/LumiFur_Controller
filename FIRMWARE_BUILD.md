# LumiFur Controller Firmware Build System

This document explains the automated firmware build system for the LumiFur Controller ESP32 project.

## Overview

The firmware build system uses GitHub Actions to automatically compile and package firmware for multiple ESP32 hardware configurations. The system generates all necessary files for both OTA (Over-The-Air) updates and complete device flashing.

## Workflow Triggers

The firmware build workflow (`build-firmware.yml`) automatically runs on:

- **Push to main branch** - Builds firmware for continuous integration
- **Pull requests** - Validates that changes don't break builds
- **Git tags starting with 'v'** - Creates release artifacts (e.g., `v1.0.0`, `v2.1.3`)
- **GitHub releases** - Automatically attaches firmware files to the release

## Supported Environments

The build system compiles firmware for three ESP32 environments:

### 1. `adafruit_matrixportal_esp32s3` (Primary Target)
- **Hardware**: Adafruit MatrixPortal ESP32-S3
- **Features**: Built-in APDS9960, LIS3DH, NeoPixel, microphone
- **Use Case**: Primary production target for LumiFur Controller

### 2. `esp32dev` (Generic ESP32)
- **Hardware**: Generic ESP32 development boards
- **Features**: Basic ESP32 functionality
- **Use Case**: Development and testing on standard ESP32 boards

### 3. `dev` (Development Build)
- **Hardware**: Same as primary target
- **Features**: Extra debug instrumentation and logging
- **Use Case**: Development, debugging, and troubleshooting

## Generated Artifacts

For each environment, the build system creates:

### Essential Firmware Files
- **`firmware.bin`** - Main application firmware
  - Contains the complete LumiFur Controller application
  - Used for OTA updates
  - Flash address: 0x10000

- **`partitions.bin`** - ESP32 partition table
  - Defines memory layout for OTA, NVS, and app partitions
  - Required for OTA update functionality
  - Flash address: 0x8000

- **`bootloader.bin`** - ESP32 bootloader
  - First-stage bootloader for ESP32
  - Handles initial boot process and OTA switching
  - Flash address: 0x1000

### Additional Files
- **`firmware.elf`** - Debug symbols (when available)
  - Contains debugging information
  - Used with debuggers like OpenOCD or ESP32 IDF Monitor
  - Not required for device operation

- **`build-info.txt`** - Build metadata
  - Git commit, branch, and tag information
  - Build timestamp and environment details
  - Useful for version tracking and support

## Installation Methods

### Method 1: OTA Update (Recommended for Updates)

If you already have LumiFur Controller firmware running:

1. Download `firmware.bin` for your hardware environment
2. Use the BLE companion app or BLE interface to initiate OTA update
3. Upload the firmware.bin file
4. Device will automatically reboot with new firmware

**Advantages**: 
- No physical connection required
- Can be done remotely
- Preserves user settings

**Requirements**:
- Device must already have LumiFur Controller firmware
- Stable power supply during update
- BLE connection to device

### Method 2: Complete Flash (First Installation or Recovery)

For first-time installation or recovery from corrupted firmware:

**Using esptool.py**:
```bash
# Install esptool if not already installed
pip install esptool

# Flash all components
esptool.py --chip esp32s3 --port /dev/ttyUSB0 --baud 921600 write_flash \
  0x1000 bootloader.bin \
  0x8000 partitions.bin \
  0x10000 firmware.bin
```

**Using ESP32 Flash Download Tools**:
1. Open ESP32 Flash Download Tools
2. Add three files with addresses:
   - `bootloader.bin` @ 0x1000
   - `partitions.bin` @ 0x8000
   - `firmware.bin` @ 0x10000
3. Select correct COM port and chip type
4. Click "START" to flash

**Using PlatformIO**:
```bash
# For development and source builds
pio run -e adafruit_matrixportal_esp32s3 --target upload
```

### Method 3: Development Build

For firmware development and customization:

```bash
# Clone repository
git clone https://github.com/stef1949/LumiFur_Controller.git
cd LumiFur_Controller

# Install PlatformIO
pip install platformio

# Build firmware
pio run -e adafruit_matrixportal_esp32s3

# Flash to device
pio run -e adafruit_matrixportal_esp32s3 --target upload

# Monitor serial output
pio device monitor -b 115200
```

## Release Process

### Automatic Release Creation

When a git tag is pushed (e.g., `git tag v1.2.0 && git push --tags`):

1. **Build Phase**: Firmware is compiled for all environments
2. **Packaging Phase**: All artifacts are bundled into release archives
3. **Upload Phase**: Archives are attached to the GitHub release
4. **Documentation Phase**: Release notes are auto-generated

### Manual Release Creation

1. Create a new release on GitHub
2. Tag the release with version number (e.g., `v1.2.0`)
3. Firmware build workflow automatically triggers
4. Artifacts are generated and attached to the release

### Release Bundle Contents

Each release includes:
- `lumifur-firmware-v1.2.0.tar.gz` - Compressed archive
- `lumifur-firmware-v1.2.0.zip` - Windows-compatible archive

Archive structure:
```
adafruit_matrixportal_esp32s3/
  ├── firmware.bin
  ├── partitions.bin
  ├── bootloader.bin
  ├── firmware.elf
  └── build-info.txt
esp32dev/
  ├── firmware.bin
  ├── partitions.bin
  ├── bootloader.bin
  ├── firmware.elf
  └── build-info.txt
dev/
  ├── firmware.bin
  ├── partitions.bin
  ├── bootloader.bin
  ├── firmware.elf
  └── build-info.txt
README.txt
```

## Troubleshooting

### Build Failures

If the firmware build fails:

1. **Check Dependencies**: Ensure all required libraries are available
2. **Platform Issues**: Verify ESP32 platform can be installed
3. **Code Issues**: Check for compilation errors in source code
4. **Environment Config**: Validate platformio.ini environment settings

### Flashing Issues

If firmware flashing fails:

1. **Port Selection**: Ensure correct USB/serial port is selected
2. **Driver Issues**: Install ESP32 USB-to-serial drivers
3. **Power Issues**: Ensure stable power supply during flashing
4. **Boot Mode**: Some boards require holding BOOT button during flash
5. **File Integrity**: Verify downloaded firmware files are not corrupted

### OTA Update Issues

If OTA updates fail:

1. **Power Supply**: Ensure stable power during update
2. **Connection**: Maintain stable BLE connection
3. **Partition Space**: Verify sufficient free space for new firmware
4. **Compatibility**: Ensure firmware compatibility with current bootloader

## Build System Maintenance

### Adding New Environments

To add support for new ESP32 hardware:

1. Add new environment to `platformio.ini`
2. Update the build matrix in `build-firmware.yml`
3. Test build locally with `pio run -e new-environment`
4. Update documentation with new hardware details

### Updating Dependencies

Library updates are managed in `platformio.ini`:

```ini
lib_deps = 
    adafruit/Adafruit GFX Library@^1.12.0
    # Update version numbers as needed
```

### Modifying Build Process

The build workflow can be customized by editing `.github/workflows/build-firmware.yml`:

- Add new build steps
- Modify artifact collection
- Change trigger conditions
- Update release packaging

## Security Considerations

- Firmware files should be verified before flashing
- OTA updates include integrity checks
- Build process is fully automated and reproducible
- All builds include full audit trail with git commit information

## Support

For issues related to firmware builds or installation:

1. Check existing [GitHub Issues](https://github.com/stef1949/LumiFur_Controller/issues)
2. Review build logs in GitHub Actions
3. Verify hardware compatibility
4. Consult the main [README.md](README.md) for general setup information

---

*This build system is designed to make firmware distribution and updates as smooth as possible for the LumiFur Controller project. The automated approach ensures consistent, reliable firmware builds for all supported hardware configurations.*