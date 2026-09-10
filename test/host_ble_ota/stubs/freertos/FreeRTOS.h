#pragma once
#include <cstdint>
using TickType_t = uint32_t;
using BaseType_t = int;
using UBaseType_t = unsigned;
constexpr BaseType_t pdPASS = 1;
#define pdMS_TO_TICKS(ms) (ms)
