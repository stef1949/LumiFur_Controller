#include "core/TemperatureHistory.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace telemetry
{
int16_t temperatureToFixed10(float celsius)
{
  if (!std::isfinite(celsius)) return 0;
  const float scaled = std::max(static_cast<float>(INT16_MIN),
                                std::min(static_cast<float>(INT16_MAX), celsius * 10.0f));
  return static_cast<int16_t>(std::lround(scaled));
}

void TemperatureHistory::clear()
{
  std::memset(samples_, 0, sizeof(samples_));
  next_ = count_ = 0;
  newestSampleMs_ = 0;
}

void TemperatureHistory::append(int16_t fixed10, uint32_t nowMs)
{
  samples_[next_] = fixed10;
  next_ = (next_ + 1) % kHistoryCapacity;
  count_ = std::min(count_ + 1, kHistoryCapacity);
  newestSampleMs_ = nowMs;
}

TemperatureSnapshot TemperatureHistory::snapshot() const
{
  TemperatureSnapshot result;
  result.count = count_;
  result.newestSampleMs = newestSampleMs_;
  const size_t oldest = count_ == kHistoryCapacity ? next_ : 0;
  for (size_t i = 0; i < count_; ++i) result.samples[i] = samples_[(oldest + i) % kHistoryCapacity];
  return result;
}

size_t historyChunkCount(const TemperatureSnapshot &snapshot)
{
  return (snapshot.count + kHistorySamplesPerChunk - 1) / kHistorySamplesPerChunk;
}

size_t encodeHistoryStart(const TemperatureSnapshot &snapshot, uint32_t nowMs, uint8_t *out, size_t capacity)
{
  if (!out || capacity < 8 || snapshot.count > kHistoryCapacity) return 0;
  const uint32_t spanSeconds = snapshot.count ? (snapshot.count - 1) * kTemperatureIntervalSeconds : 0;
  const uint32_t ageSeconds = snapshot.count ? (nowMs - snapshot.newestSampleMs) / 1000u : 0;
  const uint16_t ageMinutes = static_cast<uint16_t>(std::min<uint32_t>(UINT16_MAX, (spanSeconds + ageSeconds + 30u) / 60u));
  const uint8_t packet[] = {0xA0, 1, kTemperatureIntervalSeconds, 0,
                          static_cast<uint8_t>(snapshot.count), static_cast<uint8_t>(snapshot.count >> 8),
                          static_cast<uint8_t>(ageMinutes), static_cast<uint8_t>(ageMinutes >> 8)};
  std::memcpy(out, packet, sizeof(packet));
  return sizeof(packet);
}

size_t encodeHistoryChunk(const TemperatureSnapshot &snapshot, size_t chunk, uint8_t *out, size_t capacity)
{
  if (snapshot.count > kHistoryCapacity || chunk >= historyChunkCount(snapshot)) return 0;
  const size_t offset = chunk * kHistorySamplesPerChunk;
  const size_t count = std::min<size_t>(kHistorySamplesPerChunk, snapshot.count - offset);
  const size_t length = 4 + count * sizeof(int16_t);
  if (!out || capacity < length) return 0;
  out[0] = 0xA1;
  out[1] = static_cast<uint8_t>(chunk);
  out[2] = static_cast<uint8_t>(historyChunkCount(snapshot));
  out[3] = static_cast<uint8_t>(count);
  for (size_t i = 0; i < count; ++i)
  {
    const uint16_t value = static_cast<uint16_t>(snapshot.samples[offset + i]);
    out[4 + i * 2] = static_cast<uint8_t>(value);
    out[5 + i * 2] = static_cast<uint8_t>(value >> 8);
  }
  return length;
}
}
