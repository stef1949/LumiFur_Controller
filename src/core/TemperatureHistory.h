#pragma once

#include <cstddef>
#include <cstdint>

namespace telemetry
{
constexpr uint8_t kTemperatureIntervalSeconds = 1;
constexpr uint32_t kTemperatureIntervalMs = kTemperatureIntervalSeconds * 1000u;
constexpr size_t kHistoryCapacity = 50;
constexpr uint8_t kHistorySamplesPerChunk = 8; // 20 bytes, fits the default ATT MTU.
constexpr size_t kHistoryPacketMaxBytes = 4 + kHistorySamplesPerChunk * sizeof(int16_t);

int16_t temperatureToFixed10(float celsius);

struct TemperatureSnapshot
{
  int16_t samples[kHistoryCapacity] = {};
  size_t count = 0;
  uint32_t newestSampleMs = 0;
};

// Caller synchronizes access; packet encoding operates on an immutable snapshot.
class TemperatureHistory
{
public:
  void clear();
  void append(int16_t fixed10, uint32_t nowMs);
  TemperatureSnapshot snapshot() const;

private:
  int16_t samples_[kHistoryCapacity] = {};
  size_t next_ = 0;
  size_t count_ = 0;
  uint32_t newestSampleMs_ = 0;
};

size_t historyChunkCount(const TemperatureSnapshot &snapshot);
size_t encodeHistoryStart(const TemperatureSnapshot &snapshot, uint32_t nowMs, uint8_t *out, size_t capacity);
size_t encodeHistoryChunk(const TemperatureSnapshot &snapshot, size_t chunk, uint8_t *out, size_t capacity);
}
