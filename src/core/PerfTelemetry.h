#pragma once

#include <Arduino.h>

#include <cstddef>
#include <cstdint>

enum class PerfDurationId : uint8_t
{
  RenderFrame = 0,
  BleCallback,
  MicBlock,
  ApdsTransaction,
  DisplayTaskBusy,
  BleWorkerTaskBusy,
  Count
};

struct PerfDurationStatsSnapshot
{
  uint32_t calls = 0;
  uint64_t totalMicros = 0;
  uint32_t maxMicros = 0;
};

struct PerfTelemetrySnapshot
{
  PerfDurationStatsSnapshot durations[static_cast<size_t>(PerfDurationId::Count)] = {};
  uint32_t slowFrames = 0;
};

void perfTelemetryRecordDuration(PerfDurationId id, uint32_t elapsedMicros);
void perfTelemetryRecordSlowFrame();
PerfTelemetrySnapshot perfTelemetryTakeSnapshot();

struct ScopedPerfTelemetryDuration
{
  PerfDurationId id;
  uint32_t startMicros;

  explicit ScopedPerfTelemetryDuration(PerfDurationId metricId)
      : id(metricId), startMicros(micros())
  {
  }

  ~ScopedPerfTelemetryDuration()
  {
    perfTelemetryRecordDuration(id, micros() - startMicros);
  }
};
