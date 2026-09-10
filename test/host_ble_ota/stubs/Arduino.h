#pragma once
#include <cstdint>
struct SerialStub {
    void println(const char*) {}
    template <typename... Args> void printf(const char*, Args...) {}
};
extern SerialStub Serial;
inline uint32_t micros() { return 0; }
