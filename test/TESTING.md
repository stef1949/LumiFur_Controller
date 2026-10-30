# Testing LumiFur Controller

Run the production core suites with PlatformIO:

```sh
pio test -e native2          # Linux/macOS
pio test -e native2_windows  # Windows with GCC in PATH
```

The native source filter builds AnimationState, ColorParser, ScrollState,
TemperatureHistory, MonoVideoCodec, microphone math and MouthMorph. Header-based
input debouncing, power policy, motion sampling and easing are tested directly.
The former duplicated timer/threshold suites have been replaced by input and
power-policy suites; BLE history tests encode the production fixed10 protocol.
The microphone pipeline suite calls production microphone math.

Host controller checks require Python 3.10+, GCC with C++17 support, and Node 18+:

```sh
python -m unittest discover -s tools -p 'test_*.py'
python -m unittest discover -s docs/firmware-updater/tests
node --test docs/firmware-updater/tests/ota-transfer.test.js
```

These compile production controller sources with hardware fakes. They exercise
OTA reboot, encryption checks, flash errors, queue overflow, connection ownership,
stale queued packets, disconnect cleanup, timeout rollover, sleep/button wake,
configuration changes, offline temperature sampling, immutable history transfers,
face rendering and HUB75 DMA synchronization. The JavaScript tests exercise the
browser transport, including acknowledgement ordering, errors, write deadlines,
offset validation, disconnection and legacy firmware.

`test/run_unity_tests.sh` is a GCC fallback when PlatformIO is unavailable. Set
`UNITY_DIR` to an existing Unity checkout to avoid downloading one.

Build the supported MatrixPortal configurations before release:

```sh
pio run -e adafruit_matrixportal_esp32s3 -e adafruit_matrixportal_esp32s3_waveshare -e dev
```

Host fakes cannot validate physical DMA timing, I2C behaviour, Bluetooth throughput,
or the reboot on the controller. Check those on hardware before distributing a
release. No test command flashes a connected device.
