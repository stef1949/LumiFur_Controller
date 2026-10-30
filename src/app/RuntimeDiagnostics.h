#pragma once
#include <Arduino.h>
#include "config/debug_config.h"
#include "perf_tuning.h"
#ifndef PERF_MONITORING
#define PERF_MONITORING LF_PERF_MONITORING_DEFAULT // Set to 1 to collect runtime timing/heap metrics
#endif
#ifndef PERF_LOGGING
#define PERF_LOGGING LF_PERF_LOGGING_DEFAULT // Set to 1 to emit periodic perf logs
#endif
#ifndef PERF_REPORT_INTERVAL_MS
#define PERF_REPORT_INTERVAL_MS 1000UL
#endif
#ifndef PERF_SELF_TEST
#define PERF_SELF_TEST 0
#endif

#if DEBUG_MODE
#define LOG_DEBUG(...) Serial.printf(__VA_ARGS__)
#define LOG_DEBUG_LN(message) Serial.println(message)
#else
#define LOG_DEBUG(...) \
  do                   \
  {                    \
  } while (0)
#define LOG_DEBUG_LN(message) \
  do                          \
  {                           \
  } while (0)
#endif


#if DEBUG_MODE
constexpr uint32_t SLOW_SECTION_THRESHOLD_US = 2000; // Flag non-render work that takes >2ms
#define CONCAT_INNER(a, b) a##b
#define CONCAT(a, b) CONCAT_INNER(a, b)
struct ScopedSectionTimer
{
  const char *label;
  const uint32_t startMicros;
  ScopedSectionTimer(const char *name) : label(name), startMicros(micros()) {}
  ~ScopedSectionTimer()
  {
    const uint32_t elapsed = micros() - startMicros;
    if (elapsed > SLOW_SECTION_THRESHOLD_US)
    {
      Serial.printf("Slow section %s: %lu us\n", label, static_cast<unsigned long>(elapsed));
    }
  }
};
#define PROFILE_SECTION(name) ScopedSectionTimer CONCAT(sectionTimer_, __LINE__)(name)
#else
#define PROFILE_SECTION(name) \
  do                          \
  {                           \
  } while (0)
#endif

#if PERF_MONITORING
enum class PerfBucket : uint8_t
{
  Render = 0,
  Ble,
  Sensors,
  Io,
  Animation,
  Count
};

void perfRecordLoopDuration(uint32_t elapsedMicros);
void perfRecordBucket(PerfBucket bucket, uint32_t elapsedMicros);
void perfMaybeReport(unsigned long nowMs);
struct ScopedPerfBucketTimer
{
  const PerfBucket bucket;
  const uint32_t startMicros;

  explicit ScopedPerfBucketTimer(PerfBucket perfBucket)
      : bucket(perfBucket), startMicros(micros())
  {
  }

  ~ScopedPerfBucketTimer()
  {
    perfRecordBucket(bucket, micros() - startMicros);
  }
};

#define PERF_CONCAT_INNER(a,b) a##b
#define PERF_CONCAT(a,b) PERF_CONCAT_INNER(a,b)
#define PERF_SCOPE(bucket) ScopedPerfBucketTimer PERF_CONCAT(perfScope_, __LINE__)(bucket)
#else
inline void perfRecordLoopDuration(uint32_t) {}
inline void perfMaybeReport(unsigned long) {}
#define PERF_SCOPE(bucket) do {} while (0)
#endif
void runPerfSelfTest();
