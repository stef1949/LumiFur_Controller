#include "ble/ble_scroll.h"

#include "core/ScrollState.h"

#include <Arduino.h>
#include <cstring>

void ScrollSpeedCallbacks::onWrite(NimBLECharacteristic *pChr, NimBLEConnInfo &connInfo)
{
    const auto &val = pChr->getValue(); // getValue() returns a temporary; bind as const reference
    if (val.size() >= 1)
    {
        uint8_t incoming = static_cast<uint8_t>(val[0]);
        if (incoming < 1)
            incoming = 1;
        if (incoming > 100)
            incoming = 100;
        auto &scrollState = scroll::state();
        scrollState.speedSetting = incoming;
        scroll::updateIntervalFromSpeed();
#if DEBUG_MODE
        Serial.printf("BLE: New scroll speed=%u (interval=%u ms)\n", scrollState.speedSetting, scrollState.textIntervalMs);
#endif
        // Echo back & notify (optional)
        pChr->setValue(&scrollState.speedSetting, 1);
        pChr->notify();
    }
}

void ScrollSpeedCallbacks::onRead(NimBLECharacteristic *pChr, NimBLEConnInfo &connInfo)
{
    auto &scrollState = scroll::state();
    pChr->setValue(&scrollState.speedSetting, 1);
}

void ScrollTextCallbacks::onWrite(NimBLECharacteristic *pChr, NimBLEConnInfo &connInfo)
{
    extern char txt[64]; // Declare external txt buffer
    const auto &val = pChr->getValue();
    if (val.size() > 0 && val.size() < sizeof(txt))
    {
        // Copy new message to txt buffer
        memset(txt, 0, sizeof(txt)); // Clear existing text
        memcpy(txt, val.data(), val.size());
        txt[val.size()] = '\0'; // Ensure null termination

        // Reinitialize scrolling text with new message
        extern void initializeScrollingText();
        initializeScrollingText();

#if DEBUG_MODE
        Serial.printf("BLE: New scroll text received: '%s' (%u bytes)\n", txt, val.size());
#endif

        // Echo back & notify (optional)
        pChr->setValue(txt);
        pChr->notify();
    }
    else if (val.size() >= sizeof(txt))
    {
        Serial.printf("BLE: Scroll text too long! Max %u bytes, got %u\n", sizeof(txt) - 1, val.size());
    }
}

void ScrollTextCallbacks::onRead(NimBLECharacteristic *pChr, NimBLEConnInfo &connInfo)
{
    extern char txt[64]; // ADD THIS LINE - Declare extern to access the global txt buffer
                         // Return current text when app reads
    pChr->setValue(txt);
#if DEBUG_MODE
    Serial.printf("BLE: Scroll text read: '%s'\n", txt);
#endif
}
