#include "app/RuntimeDiagnostics.h"
#include "app/Views.h"
#include "app/RuntimeTuning.h"
#include "app/ViewManager.h"
#include "app/InputController.h"
#include "ble/BleStatus.h"
#include "ble/ble_worker.h"
#include "core/PerfTelemetry.h"
#include "core/mic/mic.h"
#include "app/AppRuntime.h"
#include <cstring>

#if PERF_MONITORING
struct PerfBucketStats
{
  uint32_t calls = 0;
  uint64_t totalMicros = 0;
  uint32_t maxMicros = 0;
};

struct PerfCounters
{
  uint32_t loopCount = 0;
  uint64_t loopTotalMicros = 0;
  uint32_t loopMaxMicros = 0;
  PerfBucketStats buckets[static_cast<size_t>(PerfBucket::Count)];
};

static portMUX_TYPE gPerfMux = portMUX_INITIALIZER_UNLOCKED;
static PerfCounters gPerfCounters;
static unsigned long gPerfLastReportMs = 0;

void perfRecordLoopDuration(uint32_t elapsedMicros)
{
  portENTER_CRITICAL(&gPerfMux);
  ++gPerfCounters.loopCount;
  gPerfCounters.loopTotalMicros += elapsedMicros;
  if (elapsedMicros > gPerfCounters.loopMaxMicros)
  {
    gPerfCounters.loopMaxMicros = elapsedMicros;
  }
  portEXIT_CRITICAL(&gPerfMux);
}

void perfRecordBucket(PerfBucket bucket, uint32_t elapsedMicros)
{
  const size_t index = static_cast<size_t>(bucket);
  portENTER_CRITICAL(&gPerfMux);
  PerfBucketStats &stats = gPerfCounters.buckets[index];
  ++stats.calls;
  stats.totalMicros += elapsedMicros;
  if (elapsedMicros > stats.maxMicros)
  {
    stats.maxMicros = elapsedMicros;
  }
  portEXIT_CRITICAL(&gPerfMux);
}

void perfMaybeReport(unsigned long nowMs)
{
#if PERF_LOGGING
  if ((nowMs - gPerfLastReportMs) < PERF_REPORT_INTERVAL_MS)
  {
    return;
  }

  PerfCounters snapshot;
  portENTER_CRITICAL(&gPerfMux);
  snapshot = gPerfCounters;
  std::memset(&gPerfCounters, 0, sizeof(gPerfCounters));
  portEXIT_CRITICAL(&gPerfMux);

  gPerfLastReportMs = nowMs;

  const uint32_t freeHeap = ESP.getFreeHeap();
  const uint32_t minFreeHeap = ESP.getMinFreeHeap();
  const PerfTelemetrySnapshot telemetry = perfTelemetryTakeSnapshot();
  const BleWorkQueueStats bleQueueStats = bleTakeQueueStats();
  const unsigned long queueDepth = bleQueueDepth();
  const unsigned long bleQueueHighWater = static_cast<unsigned long>(bleQueueStats.highWaterDepth);
  const unsigned long bleQueueDrops = static_cast<unsigned long>(bleQueueStats.drops);
  const unsigned long displayStackMin = displayTaskStackHighWaterMark();
  const unsigned long bleNotifyStackMin = bleNotificationStackHighWaterMark();
  const unsigned long bleWorkerStackMin = bleWorkerTaskHandle ? static_cast<unsigned long>(uxTaskGetStackHighWaterMark(bleWorkerTaskHandle)) : 0UL;
  const unsigned long micStackMin = static_cast<unsigned long>(micGetTaskStackHighWaterMark());

  const auto avgMicros = [](uint64_t total, uint32_t count) -> uint32_t
  {
    if (count == 0)
    {
      return 0;
    }
    return static_cast<uint32_t>(total / count);
  };

  const PerfBucketStats &render = snapshot.buckets[static_cast<size_t>(PerfBucket::Render)];
  const PerfBucketStats &ble = snapshot.buckets[static_cast<size_t>(PerfBucket::Ble)];
  const PerfBucketStats &sensors = snapshot.buckets[static_cast<size_t>(PerfBucket::Sensors)];
  const PerfBucketStats &io = snapshot.buckets[static_cast<size_t>(PerfBucket::Io)];
  const PerfBucketStats &animation = snapshot.buckets[static_cast<size_t>(PerfBucket::Animation)];
  const PerfDurationStatsSnapshot &renderFrame = telemetry.durations[static_cast<size_t>(PerfDurationId::RenderFrame)];
  const PerfDurationStatsSnapshot &bleCallback = telemetry.durations[static_cast<size_t>(PerfDurationId::BleCallback)];
  const PerfDurationStatsSnapshot &micBlock = telemetry.durations[static_cast<size_t>(PerfDurationId::MicBlock)];
  const PerfDurationStatsSnapshot &apdsTxn = telemetry.durations[static_cast<size_t>(PerfDurationId::ApdsTransaction)];
  const PerfDurationStatsSnapshot &displayBusy = telemetry.durations[static_cast<size_t>(PerfDurationId::DisplayTaskBusy)];
  const PerfDurationStatsSnapshot &bleWorkerBusy = telemetry.durations[static_cast<size_t>(PerfDurationId::BleWorkerTaskBusy)];

  Serial.printf(
      "PERF loop=%lu avg=%luus max=%luus render=%lu/%luus/%luus ble=%lu/%luus/%luus sens=%lu/%luus/%luus io=%lu/%luus/%luus anim=%lu/%luus/%luus frame=%lu/%luus/%luus slow=%lu blecb=%lu/%luus/%luus mic=%lu/%luus/%luus apds=%lu/%luus/%luus disp=%lu/%luus/%luus blewrk=%lu/%luus/%luus q=%lu qhi=%lu qdrop=%lu stkD=%lu stkN=%lu stkW=%lu stkM=%lu heap=%lu min=%lu\n",
      static_cast<unsigned long>(snapshot.loopCount),
      static_cast<unsigned long>(avgMicros(snapshot.loopTotalMicros, snapshot.loopCount)),
      static_cast<unsigned long>(snapshot.loopMaxMicros),
      static_cast<unsigned long>(render.calls),
      static_cast<unsigned long>(avgMicros(render.totalMicros, render.calls)),
      static_cast<unsigned long>(render.maxMicros),
      static_cast<unsigned long>(ble.calls),
      static_cast<unsigned long>(avgMicros(ble.totalMicros, ble.calls)),
      static_cast<unsigned long>(ble.maxMicros),
      static_cast<unsigned long>(sensors.calls),
      static_cast<unsigned long>(avgMicros(sensors.totalMicros, sensors.calls)),
      static_cast<unsigned long>(sensors.maxMicros),
      static_cast<unsigned long>(io.calls),
      static_cast<unsigned long>(avgMicros(io.totalMicros, io.calls)),
      static_cast<unsigned long>(io.maxMicros),
      static_cast<unsigned long>(animation.calls),
      static_cast<unsigned long>(avgMicros(animation.totalMicros, animation.calls)),
      static_cast<unsigned long>(animation.maxMicros),
      static_cast<unsigned long>(renderFrame.calls),
      static_cast<unsigned long>(avgMicros(renderFrame.totalMicros, renderFrame.calls)),
      static_cast<unsigned long>(renderFrame.maxMicros),
      static_cast<unsigned long>(telemetry.slowFrames),
      static_cast<unsigned long>(bleCallback.calls),
      static_cast<unsigned long>(avgMicros(bleCallback.totalMicros, bleCallback.calls)),
      static_cast<unsigned long>(bleCallback.maxMicros),
      static_cast<unsigned long>(micBlock.calls),
      static_cast<unsigned long>(avgMicros(micBlock.totalMicros, micBlock.calls)),
      static_cast<unsigned long>(micBlock.maxMicros),
      static_cast<unsigned long>(apdsTxn.calls),
      static_cast<unsigned long>(avgMicros(apdsTxn.totalMicros, apdsTxn.calls)),
      static_cast<unsigned long>(apdsTxn.maxMicros),
      static_cast<unsigned long>(displayBusy.calls),
      static_cast<unsigned long>(avgMicros(displayBusy.totalMicros, displayBusy.calls)),
      static_cast<unsigned long>(displayBusy.maxMicros),
      static_cast<unsigned long>(bleWorkerBusy.calls),
      static_cast<unsigned long>(avgMicros(bleWorkerBusy.totalMicros, bleWorkerBusy.calls)),
      static_cast<unsigned long>(bleWorkerBusy.maxMicros),
      queueDepth,
      bleQueueHighWater,
      bleQueueDrops,
      displayStackMin,
      bleNotifyStackMin,
      bleWorkerStackMin,
      micStackMin,
      static_cast<unsigned long>(freeHeap),
      static_cast<unsigned long>(minFreeHeap));
#else
  (void)nowMs;
#endif
}
#endif

void runPerfSelfTest()
{
  bool passed = true;

  if (runtime::kBaseFrameIntervalMs == 0)
  {
    Serial.println("SELFTEST: base frame interval must be > 0");
    passed = false;
  }

  if (runtime::kProximityReleaseDelta > runtime::kProximityTriggerDelta)
  {
    Serial.println("SELFTEST: proximity hysteresis is inverted");
    passed = false;
  }

  if (sizeof(txt) < 2)
  {
    Serial.println("SELFTEST: scroll text buffer is unexpectedly small");
    passed = false;
  }

  if (TOTAL_VIEWS <= 0)
  {
    Serial.println("SELFTEST: no views are registered");
    passed = false;
  }

  Serial.printf("SELFTEST: %s heap=%lu min=%lu\n",
                passed ? "PASS" : "FAIL",
                static_cast<unsigned long>(ESP.getFreeHeap()),
                static_cast<unsigned long>(ESP.getMinFreeHeap()));
}
