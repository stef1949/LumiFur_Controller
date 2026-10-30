#include "app/InputController.h"
#include "app/AppRuntime.h"
#include "app/ViewState.h"
#include "app/PowerManagement.h"
#include "core/InputState.h"
#include "ble/ble_state.h"
#include "hardware/deviceConfig.h"
#include <Arduino.h>

using namespace input;
namespace
{
constexpr unsigned long BUTTON_DEBOUNCE_MS = 50;
constexpr unsigned long PAIRING_MODE_HOLD_MS = 3000;
constexpr unsigned long PAIRING_RESET_HOLD_MS = 3000;
}

void handleButtonInputs(unsigned long loopNow)
{
// --- Handle button inputs for view changes and BLE pairing ---
#if defined(BUTTON_UP) || defined(BUTTON_DOWN)
#ifdef BUTTON_UP
    static ButtonState upButtonState;
#endif
#ifdef BUTTON_DOWN
    static ButtonState downButtonState;
#endif
#if defined(BUTTON_UP) && defined(BUTTON_DOWN)
    static bool pairingHoldActive = false;
    static bool pairingHoldTriggered = false;
    static unsigned long pairingHoldStartMs = 0;
#endif

#ifdef BUTTON_UP
    updateButtonState(upButtonState, digitalRead(BUTTON_UP) == LOW, loopNow, BUTTON_DEBOUNCE_MS);
#endif
#ifdef BUTTON_DOWN
    updateButtonState(downButtonState, digitalRead(BUTTON_DOWN) == LOW, loopNow, BUTTON_DEBOUNCE_MS);
#endif

#ifdef BUTTON_UP
    const bool upPressed = upButtonState.stablePressed;
#endif
#ifdef BUTTON_DOWN
    const bool downPressed = downButtonState.stablePressed;
#endif
    bool wakePressed = false;
#ifdef BUTTON_UP
    wakePressed = wakePressed || upPressed;
#endif
#ifdef BUTTON_DOWN
    wakePressed = wakePressed || downPressed;
#endif
    if (wakePressed)
    {
      wakeFromSleepMode();
      noteButtonInteraction(loopNow);
      lastActivityTime = loopNow;
    }
#if defined(BUTTON_UP) && defined(BUTTON_DOWN)
    const bool pairingResetHoldPressed = upPressed && downPressed;
#else
    const bool pairingResetHoldPressed = false;
#endif

#if defined(BUTTON_UP) && defined(BUTTON_DOWN)
    if (pairingResetHoldPressed)
    {
      suppressButtonPress(upButtonState);
      suppressButtonPress(downButtonState);
      noteButtonInteraction(loopNow);
      lastActivityTime = loopNow;
      if (!pairingHoldActive)
      {
        pairingHoldActive = true;
        pairingHoldStartMs = loopNow;
      }
      else if (!pairingHoldTriggered && (loopNow - pairingHoldStartMs >= PAIRING_RESET_HOLD_MS))
      {
        pairingHoldTriggered = true;
        resetBlePairing();
      }
    }
    else
    {
      pairingHoldActive = false;
      pairingHoldTriggered = false;
    }
#endif

    bool pairingModeTriggered = false;

    auto enablePairingModeFromHeldButton = [&]()
    {
      noteButtonInteraction(loopNow);
      startPairingMode(PAIRING_MODE_WINDOW_MS);
      refreshBleAdvertising();
      lastActivityTime = loopNow;
      pairingModeTriggered = true;
      requestDisplayRefresh();
    };

#ifdef BUTTON_UP
    if (!pairingResetHoldPressed && upPressed &&
        consumeButtonLongPress(upButtonState, loopNow, PAIRING_MODE_HOLD_MS))
    {
      enablePairingModeFromHeldButton();
    }
#endif

#ifdef BUTTON_DOWN
    if (!pairingResetHoldPressed && downPressed &&
        consumeButtonLongPress(downButtonState, loopNow, PAIRING_MODE_HOLD_MS))
    {
      enablePairingModeFromHeldButton();
    }
#endif

#ifdef BUTTON_UP
    if (!pairingResetHoldPressed && consumeButtonShortPress(upButtonState))
    {
      noteButtonInteraction(loopNow);
      selectUserView(static_cast<uint8_t>((currentView + 1) % totalViews));
      lastActivityTime = loopNow;
    }
#endif

#ifdef BUTTON_DOWN
    if (!pairingResetHoldPressed && consumeButtonShortPress(downButtonState))
    {
      noteButtonInteraction(loopNow);
      selectUserView(currentView == 0 ? static_cast<uint8_t>(totalViews - 1) : static_cast<uint8_t>(currentView - 1));
      lastActivityTime = loopNow;
    }
#endif

    if (pairingModeTriggered)
    {
      Serial.println("BLE pairing mode enabled by long button press.");
    }


#endif

}
