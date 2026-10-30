# Application structure

`src/app/main.cpp` contains the Arduino entry points and update scheduling. It
does not implement rendering, gestures, power transitions or BLE callbacks.
`main.h` declares only those entry points; other code includes its subsystem's
header instead of importing application globals and function definitions.

| Responsibility | Implementation | Interface |
| --- | --- | --- |
| Hardware and preference initialization | `app/AppSetup.cpp` | `initializeApplication()` |
| View IDs and user/temporary selection | `app/Views.h`, `app/ViewState.cpp` | `selectUserView()`, `updateViewTimers()` |
| Renderer dispatch, fades and display task | `app/ViewManager.cpp` | `startViewManager()`, `displayCurrentView()` |
| Blink, eye hover/bounce and blush timing | `app/AnimationController.cpp` | `updateViewAnimations()` |
| Buttons, shake gestures and proximity reactions | `app/InputController.cpp` | `handleButtonInputs()`, `handleSensorInputs()` |
| Accelerometer sampling | `app/Sensors.cpp` | `getLatestAcceleration()`, `sleepMotionDetected()` |
| Sleep/wake and CPU frequency | `app/PowerManagement.cpp` | `checkSleepMode()`, `wakeFromSleepMode()` |
| GATT services and control handlers | `ble/ble_control.cpp`, `ble/ble_server.cpp` | `setupBleControl()` |
| BLE queue and OTA session lifecycle | `ble/ble_worker.cpp`, `ble/ble_ota.cpp` | `startBleWorker()` |
| BLE status light and notifications | `ble/BleStatus.cpp` | `handleBLEStatusLED()`, `startBleNotifications()` |
| Delayed view persistence | `config/ViewPersistence.cpp` | `scheduleLastViewPersist()`, `flushPendingLastViewPersist()` |
| Performance counters and instrumentation | `app/RuntimeDiagnostics.cpp` | `PERF_SCOPE`, `perfMaybeReport()` |

All paths in the table are relative to `src/`.

The renderer implementation is divided further:

- `rendering/FaceRenderer.cpp`: expression bitmaps, eyes and microphone mouth frames.
- `rendering/EffectRenderer.cpp`: effects, particles, video and their initialization/reset hooks.
- `rendering/ScrollRenderer.cpp`: text buffer, cursor and scroll timing.
- `rendering/RenderPrimitives.cpp`: generic bitmap transforms and plasma palette cache.
- `rendering/DisplayOverlays.cpp`: Bluetooth rune and pairing passkey artwork.
- `rendering/RenderState.cpp`: brightness requests, palette state and DMA presentation.

`RenderSupport.h` groups declaration headers used by renderer implementations;
it does not define mutable state or include implementation files. Renderer-specific
caches, effect objects and display/BLE task handles stay in their owning source
files. `RuntimeTuning.h` holds shared timing and proximity constants.

## Runtime ownership

The display task remains pinned to core 0. Arduino updates, BLE and microphone
work remain on core 1. `AppStateLock` protects shared view, animation, input and
render state. Release it before waiting for the next frame or task notification.
The microphone publishes its outputs atomically. The temperature history and BLE
queue have separate synchronization, and history notifications do not hold the
application lock while waiting on BLE.

Brightness updates from other tasks publish a pending value. The display task
consumes it between frames and owns normal runtime DMA presentation. Startup
presents the boot frames before that task starts.

The coordinator preserves the update order: expire pairing, flush delayed
preferences, handle buttons, then process awake animations and sensor reactions.
Temperature sampling runs in either power state. Sleep evaluation runs after
inputs so button activity can prevent an inactivity transition.

## Extending and validating

Keep BLE view IDs in `Views.h` stable. Add a view's rendering function to the
appropriate renderer and its dispatch entry to `ViewManager.cpp`. Update the
golden view-ID contract in `tools/test_view_manager.py` when intentionally adding
or changing a protocol mapping. Effect state reset belongs in `resetEffectView()`.

The host regression suite exercises the production dispatcher for every view ID,
entry resets, pairing/sleep overlays and frame budgets. Face, input, power, OTA
and DMA ownership regressions remain separate. See [testing](../test/TESTING.md)
for commands and hardware-verification limits.
