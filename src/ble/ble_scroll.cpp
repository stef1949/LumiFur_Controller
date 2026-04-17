#include "ble/ble_scroll.h"
#include "ble/ble_worker.h"

#include "core/ScrollState.h"
#include "core/PerfTelemetry.h"
#include "config/userPreferences.h"

#include <Arduino.h>
#include <cstring>

bool handleScrollWritePayload(NimBLECharacteristic *pChr, const uint8_t *data, size_t length)
{
    extern char txt[64]; // Declare external txt buffer
    if (pChr == nullptr || data == nullptr || length == 0)
    {
        return false;
    }

    constexpr uint8_t kOpcodeText = 0x01;
    constexpr uint8_t kOpcodeSpeed = 0x02;

    const uint8_t opcode = data[0];
    if (opcode == kOpcodeSpeed)
    {
        if (length < 2)
        {
            return false;
        }
        uint16_t incoming = static_cast<uint8_t>(data[1]);
        if (length >= 3)
        {
            incoming = static_cast<uint16_t>(
                static_cast<uint8_t>(data[1]) |
                (static_cast<uint16_t>(static_cast<uint8_t>(data[2])) << 8));
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
        return true;
    }

    const bool hasTextOpcode = (opcode == kOpcodeText);
    const size_t textOffset = hasTextOpcode ? 1 : 0;
    const size_t textLength = length - textOffset;

    if (textLength >= sizeof(txt))
    {
        Serial.printf("BLE: Scroll text too long! Max %u bytes, got %u\n", sizeof(txt) - 1, textLength);
        return false;
    }

    memset(txt, 0, sizeof(txt)); // Clear existing text
    if (textLength > 0)
    {
        memcpy(txt, data + textOffset, textLength);
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
    return true;
}

void ScrollTextCallbacks::onWrite(NimBLECharacteristic *pChr, NimBLEConnInfo &connInfo)
{
    (void)connInfo;
    ScopedPerfTelemetryDuration perfScope(PerfDurationId::BleCallback);

    const auto &val = pChr->getValue();
    if (val.size() == 0)
    {
        return;
    }

    if (!bleQueuePayload(BleWorkType::ScrollWrite, pChr,
                         reinterpret_cast<const uint8_t *>(val.data()), val.size()))
    {
        Serial.println("BLE: Scroll write queue full, dropping payload.");
    }
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
