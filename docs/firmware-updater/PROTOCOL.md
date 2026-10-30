# BLE firmware update protocol

The OTA characteristic is `01931c44-3867-7427-96ab-8d7ac0ae09ee` in service
`01931c44-3867-7740-9867-c822cb7df308`. Writes require an encrypted connection.
Subscribe to notifications before sending START. Integers below are unsigned
little-endian values.

| Command | Request | Successful response |
| --- | --- | --- |
| START | `01 size:u32 [01]` | `01 00`, or `01 00 01` when DATA acknowledgements were requested |
| DATA | `02 firmware-bytes...` | `02 00 received:u32` in acknowledged mode |
| END | `03` | `03 00`, after image validation and boot-partition selection |
| ABORT | `04` | `04 00` when an active session was aborted |

The optional sixth START byte opts into DATA acknowledgements. Existing five-byte
clients retain the original protocol and receive no DATA notifications. In
acknowledged mode, wait for START and for each DATA acknowledgement before sending
the next packet. Check that `received` matches the total bytes sent. Send END only
after every byte is acknowledged, and wait for its response. A GATT write response
means the request reached the queue; the OTA notification confirms the flash write.
The browser sends at most 180 firmware bytes per DATA packet.

All failures are `FF detail`. Existing details `01`–`0F` are preserved. Added
details are `10` for inactivity timeout, `11` when reboot is already pending and
`12` for a different session owner. Stop sending DATA on any error, issue ABORT
when connected, and let the operation settle before retrying. Do not automatically
resend DATA after an ambiguous timeout, since its bytes may already be in flash.

Only the connection that started a session may write or end it. Queued packets
carry a connection generation so a reused BLE handle cannot replay old work.
The BLE worker aborts the session after the owner disconnects or after 15 seconds
without accepted DATA. Cleanup is polled even when the queue is full. Successful
END schedules a restart after 250 ms; its success acknowledgement is definitive
if reboot disconnects the GATT operation immediately afterward.

The browser detects firmware without the acknowledgement capability and uses
paced legacy DATA writes for its first upgrade. It still waits for START/END,
stops on errors and bounds every write. Per-packet confirmation and automatic
firmware-side disconnect cleanup require the updated controller firmware.
