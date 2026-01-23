#include "ble/ble_scroll.h"

#include "core/ScrollState.h"
#include "userPreferences.h"

#include <Arduino.h>
#include <cstring>

void ScrollTextCallbacks::onWrite(NimBLECharacteristic *pChr, NimBLEConnInfo &connInfo)
{
    extern char txt[64]; // Declare external txt buffer
    const auto &val = pChr->getValue();
    if (val.size() == 0)
    {
        return;
    }

    constexpr uint8_t kOpcodeText = 0x01;
    constexpr uint8_t kOpcodeSpeed = 0x02;

    const uint8_t opcode = static_cast<uint8_t>(val[0]);
    if (opcode == kOpcodeSpeed)
    {
        if (val.size() < 2)
        {
            return;
        }
        uint16_t incoming = static_cast<uint8_t>(val[1]);
        if (val.size() >= 3)
        {
            incoming = static_cast<uint16_t>(
                static_cast<uint8_t>(val[1]) |
                (static_cast<uint16_t>(static_cast<uint8_t>(val[2])) << 8));
        }
        if (incoming < 1)
        {
            incoming = 1;
        }
        if (incoming > 500)
        {
            incoming = 500;
        }
        auto &scrollState = scroll::state();
        scrollState.speedSetting = incoming;
        scroll::updateIntervalFromSpeed();
        saveScrollSpeed(incoming);
#if (defined(DEBUG_MODE) && DEBUG_MODE) || (defined(TEXT_DEBUG) && TEXT_DEBUG)
        Serial.printf("BLE: New scroll speed=%u (interval=%u ms)\n",
                      static_cast<unsigned int>(scrollState.speedSetting),
                      static_cast<unsigned int>(scrollState.textIntervalMs));
#endif
        return;
    }

    const bool hasTextOpcode = (opcode == kOpcodeText);
    const size_t textOffset = hasTextOpcode ? 1 : 0;
    const size_t textLength = val.size() - textOffset;

    if (textLength >= sizeof(txt))
    {
        Serial.printf("BLE: Scroll text too long! Max %u bytes, got %u\n", sizeof(txt) - 1, textLength);
        return;
    }

    memset(txt, 0, sizeof(txt)); // Clear existing text
    if (textLength > 0)
    {
        memcpy(txt, val.data() + textOffset, textLength);
    }
    txt[textLength] = '\0';
    saveUserText(String(txt));

    // Reinitialize on the render task to avoid touching the display from BLE callbacks.
    auto &scrollState = scroll::state();
    scrollState.textInitialized = false;

#if (defined(DEBUG_MODE) && DEBUG_MODE) || (defined(TEXT_DEBUG) && TEXT_DEBUG)
    Serial.printf("BLE: New scroll text received: '%s' (%u bytes)\n", txt, textLength);
#endif

    // Echo back & notify (optional)
    pChr->setValue(txt);
    pChr->notify();
}

void ScrollTextCallbacks::onRead(NimBLECharacteristic *pChr, NimBLEConnInfo &connInfo)
{
    extern char txt[64]; // ADD THIS LINE - Declare extern to access the global txt buffer
                         // Return current text when app reads
    pChr->setValue(txt);
#if (defined(DEBUG_MODE) && DEBUG_MODE) || (defined(TEXT_DEBUG) && TEXT_DEBUG)
    Serial.printf("BLE: Scroll text read: '%s'\n", txt);
#endif
}
