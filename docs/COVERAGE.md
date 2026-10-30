# Code coverage

`pio test -e native2` (or `native2_windows`) instruments the production modules
listed in `platformio.ini` with GCC coverage. The Coveralls workflow captures
those results. Coverage is a measure of the code actually compiled by those
tests, and does not imply that the entire ESP32 application or BLE stack ran.

Native tests cover the production animation/scroll state, color parsing, motion,
input debouncing, sleep policy, easing, fixed10 temperature history encoding,
monochrome video codec, microphone math and mouth morphing. History uses the same
ring buffer and packet encoder as the firmware; tests no longer maintain a
separate float-based BLE protocol or historical microphone implementation.

The separate host regression workflow compiles production OTA, worker, power,
input, view and telemetry controllers with deterministic hardware fakes, and runs
the browser updater transport under Node. Those regression runs do not currently
contribute to the Coveralls percentage. They test integration behaviour rather
than physical peripheral timing.

To generate a local HTML report after running the native suites:

```sh
gcovr --root . --filter 'src/.*' --exclude 'test/.*' --exclude '.pio/.*' \
  --html-details coverage_report/index.html
```

Install `gcovr` first if needed. Use the test output for current suite counts.
A release also requires the firmware build matrix and hardware verification;
passing native coverage cannot establish Bluetooth, sensor or panel behaviour.
