#pragma once
#include <cstdint>
#include <string>
#include <vector>
struct NimBLEConnInfo {
    uint16_t handle = 7;
    bool encrypted = true;
    uint16_t getConnHandle() const { return handle; }
    bool isEncrypted() const { return encrypted; }
};
struct NimBLECharacteristic {
    struct Notification { std::vector<uint8_t> bytes; uint16_t handle; };
    std::string value;
    std::vector<Notification> notifications;
    const std::string& getValue() const { return value; }
    bool notify(const uint8_t* data, size_t length, uint16_t handle) {
        notifications.push_back({{data, data + length}, handle});
        return true;
    }
};
struct NimBLECharacteristicCallbacks {
    virtual ~NimBLECharacteristicCallbacks() = default;
    virtual void onWrite(NimBLECharacteristic*, NimBLEConnInfo&) {}
};
