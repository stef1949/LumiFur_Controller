# BLE firmware update protocol

OTA packets use characteristic `01931c44-3867-7427-96ab-8d7ac0ae09ee` on an encrypted connection. Subscribe to its notifications before sending START. The command characteristic is for temperature history; sending OTA there can request or clear history instead of updating firmware.

| Packet | Format | Controller notification |
| --- | --- | --- |
| START, legacy | `01` + firmware size as uint32 little endian | `01 00` after flash preparation |
| START, chunk acknowledgements requested | Legacy START + `01` | `01 01` when supported; older firmware replies `01 00` |
| DATA | `02` + firmware bytes | `02 00` after flash write, only when negotiated |
| END | `03` | `03 00` after size validation, image validation, and boot partition selection |
| ABORT | `04` | `04 00`; already idle is successful cleanup |
| Error | — | `FF` + error detail |

The updated app waits for both the ATT write response and the matching firmware notification when using writes with response. Notifications may arrive first. A transport write response alone never proves image validation succeeded. START and END allow 60 seconds; other operations allow 10 seconds. The app does not retry DATA automatically, because replaying a chunk without a sequence number would corrupt the image.

DATA payloads are limited to `min(negotiated write length, 512) - 1`, including a 19-byte payload when the write limit is 20 bytes. The controller worker's 512-byte limit includes the command byte.

## Compatibility

Older firmware advertises only writes without response. The app uses that write type when necessary, respects CoreBluetooth backpressure, and paces unacknowledged DATA writes by 60 ms (the controller's maximum requested connection interval). This fallback has no per-chunk flash confirmation; controller errors and END validation remain authoritative. Updated firmware explicitly advertises both write types and supports negotiated per-chunk acknowledgements.

Five-byte START packets retain the old notification behavior. The app also treats legacy `FF 07` during ABORT as successful cleanup. If cleanup times out or fails, it disconnects before retrying. The updated controller invalidates queued packets on owner disconnect or queue overflow, aborts flash work on the worker task, and rejects writes from another connection during an update.

The firmware that is currently running performs the reboot after an update. A controller running the old task-deletion bug may need one manual restart **after a successful END notification** to activate this fix. Subsequent updates use the corrected reboot task.

## Validation

From the controller repository:

```sh
python3 tools/test_ble_ota.py
pio run -e adafruit_matrixportal_esp32s3
```

The host test compiles the production `ble_ota.cpp` with BLE, flash and task stubs under AddressSanitizer and UndefinedBehaviorSanitizer. It covers both protocol versions, notification targeting, disconnect before and during an upload, reused connection handles, other clients, queue overflow, invalid lengths, flash failures, retry cleanup, and both scheduled and immediate reboot paths.

From the app repository, choose an installed iOS Simulator:

```sh
xcodebuild -project LumiFur.xcodeproj -scheme LumiFur \
  -destination 'platform=iOS Simulator,name=iPhone 17 Pro' \
  -only-testing:LumiFurTests/FirmwareUpdateDomainTests \
  test CODE_SIGNING_ALLOWED=NO
```

The firmware update suite is now included in the Xcode test target. Tests cover notification ordering, error/mismatched acknowledgements, protocol negotiation, packet sizing and byte reconstruction, finalization progress, legacy settings decoding, and existing update-domain behavior.

A physical BLE test remains necessary: update from old firmware, update again with chunk acknowledgements, cancel and retry mid-transfer, disconnect and reconnect mid-transfer, and confirm the advertised firmware version after reboot. Host tests and simulator builds do not establish radio throughput, flash timing, or a successful boot on hardware.

API references: [Apple write response delegate](https://developer.apple.com/documentation/corebluetooth/cbperipheraldelegate/1518823-peripheral), [ESP-IDF OTA lifecycle](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/ota.html).
