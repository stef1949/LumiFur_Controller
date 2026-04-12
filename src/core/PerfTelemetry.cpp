#include "core/PerfTelemetry.h"

#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"

namespace
{
struct PerfDurationStats
{
  uint32_t calls = 0;
  uint64_t totalMicros = 0;
  uint32_t maxMicros = 0;
};

struct PerfTelemetryCounters
{
  PerfDurationStats durations[static_cast<size_t>(PerfDurationId::Count)] = {};
  uint32_t slowFrames = 0;
};

portMUX_TYPE gPerfTelemetryMux = portMUX_INITIALIZER_UNLOCKED;
PerfTelemetryCounters gPerfTelemetryCounters;
} // namespace

void perfTelemetryRecordDuration(PerfDurationId id, uint32_t elapsedMicros)
{
  const size_t index = static_cast<size_t>(id);
  portENTER_CRITICAL(&gPerfTelemetryMux);
  PerfDurationStats &stats = gPerfTelemetryCounters.durations[index];
  ++stats.calls;
  stats.totalMicros += elapsedMicros;
  if (elapsedMicros > stats.maxMicros)
  {
    stats.maxMicros = elapsedMicros;
  }
  portEXIT_CRITICAL(&gPerfTelemetryMux);
}

void perfTelemetryRecordSlowFrame()
{
  portENTER_CRITICAL(&gPerfTelemetryMux);
  ++gPerfTelemetryCounters.slowFrames;
  portEXIT_CRITICAL(&gPerfTelemetryMux);
}

PerfTelemetrySnapshot perfTelemetryTakeSnapshot()
{
  PerfTelemetrySnapshot snapshot;
  portENTER_CRITICAL(&gPerfTelemetryMux);
  for (size_t i = 0; i < static_cast<size_t>(PerfDurationId::Count); ++i)
  {
    snapshot.durations[i].calls = gPerfTelemetryCounters.durations[i].calls;
    snapshot.durations[i].totalMicros = gPerfTelemetryCounters.durations[i].totalMicros;
    snapshot.durations[i].maxMicros = gPerfTelemetryCounters.durations[i].maxMicros;
    gPerfTelemetryCounters.durations[i] = {};
  }
  snapshot.slowFrames = gPerfTelemetryCounters.slowFrames;
  gPerfTelemetryCounters.slowFrames = 0;
  portEXIT_CRITICAL(&gPerfTelemetryMux);
  return snapshot;
}
