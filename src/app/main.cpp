#if defined(ESP_PLATFORM) && !defined(ARDUINO)
// Only include ESP-IDF headers when building as a pure ESP-IDF app, not under Arduino-ESP32.
#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/gpio.h>
#include "sdkconfig.h"
#include "esp_assert.h"
#endif

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <FFat.h>
#include <Wire.h> // For I2C sensors
#ifdef VIRTUAL_PANE
#include <ESP32-VirtualMatrixPanel-I2S-DMA.h>
#else
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#endif

#include <Adafruit_PixelDust.h> // For sand simulation
#include "main.h"
#include "effects/dinoGame.h"
#include "effects/basicRainbow.h"
#include "effects/strobeEffect.h"
#include "core/mic/mic.h"
#include "core/ColorParser.h"
#include "core/PerfTelemetry.h"
#include "effects/flameEffect.h"
#include "effects/fluidEffect.h"
#include "effects/monoVideoPlayer.h"
#include "core/AnimationState.h"
#include "core/ScrollState.h"
#include "ble/ble_worker.h"
#include "perf_tuning.h"
// #include "effects/pixelDustEffect.h" // New effect

// BLE Libraries
#include <NimBLEDevice.h>

#include <vector> //For chunking example
#include <cstring>
#include <string>
#include <cctype>
#include <cstdio>
#include <cstdlib>

#include "freertos/queue.h"
#include "freertos/task.h"

struct BleWorkQueueStats
{
  uint32_t drops = 0;
  UBaseType_t highWaterDepth = 0;
};

static TaskHandle_t displayTaskHandle = NULL;
static TaskHandle_t bleNotifyTaskHandle = NULL;
static TaskHandle_t bleWorkerTaskHandle = NULL;
static QueueHandle_t bleWorkQueue = nullptr;
static BleWorkQueueStats bleTakeQueueStats();

// #define PIXEL_COLOR_DEPTH_BITS 16 // 16 bits per pixel
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

#if PERF_MONITORING
#define PERF_CONCAT_INNER(a, b) a##b
#define PERF_CONCAT(a, b) PERF_CONCAT_INNER(a, b)

enum class PerfBucket : uint8_t
{
  Render = 0,
  Ble,
  Sensors,
  Io,
  Animation,
  Count
};

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

static void perfRecordLoopDuration(uint32_t elapsedMicros)
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

static void perfRecordBucket(PerfBucket bucket, uint32_t elapsedMicros)
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

static void perfMaybeReport(unsigned long nowMs)
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
  const unsigned long bleQueueDepth = bleWorkQueue ? static_cast<unsigned long>(uxQueueMessagesWaiting(bleWorkQueue)) : 0UL;
  const unsigned long bleQueueHighWater = static_cast<unsigned long>(bleQueueStats.highWaterDepth);
  const unsigned long bleQueueDrops = static_cast<unsigned long>(bleQueueStats.drops);
  const unsigned long displayStackMin = displayTaskHandle ? static_cast<unsigned long>(uxTaskGetStackHighWaterMark(displayTaskHandle)) : 0UL;
  const unsigned long bleNotifyStackMin = bleNotifyTaskHandle ? static_cast<unsigned long>(uxTaskGetStackHighWaterMark(bleNotifyTaskHandle)) : 0UL;
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
      bleQueueDepth,
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

#define PERF_SCOPE(bucket) ScopedPerfBucketTimer PERF_CONCAT(perfScope_, __LINE__)(bucket)
#else
inline void perfRecordLoopDuration(uint32_t elapsedMicros)
{
  (void)elapsedMicros;
}

inline void perfMaybeReport(unsigned long nowMs)
{
  (void)nowMs;
}

#define PERF_SCOPE(bucket) \
  do                       \
  {                        \
  } while (0)
#endif

// #include "EffectsLayer.hpp" // FastLED CRGB Pixel Buffer for which the patterns are drawn
// EffectsLayer effects(VPANEL_W, VPANEL_H);

// ----------------------------------------------------------------
// ----------------------------------------------------------------
// ------------------- LumiFur Global Variables -------------------
// ----------------------------------------------------------------
// ----------------------------------------------------------------
FluidEffect *fluidEffectInstance = nullptr; // Global pointer for our fluid effect object
MonoVideoPlayer *videoPlayerInstance = nullptr;
// FluidEffect* fluidEffectInstance = nullptr; // Keep or remove if replacing
// PixelDustEffect* pixelDustEffectInstance = nullptr; // Add this
// Brightness control
//  Retrieve the brightness value from preferences
bool g_accelerometer_initialized = false;
static constexpr char BAD_APPLE_VIDEO_PATH[] = "/videos/bad_apple.lfv";
static bool gVideoStorageReady = false;

constexpr unsigned long MOTION_SAMPLE_INTERVAL_FAST = 15; // ms between accel reads when looking for shakes

// --- Performance Tuning ---
// Target ~50-60 FPS. Adjust as needed based on view complexity.
const unsigned long targetFrameIntervalMillis = 8; // ~60 FPS pacing on 1 kHz ticks, ~50 FPS on 100 Hz ticks
constexpr unsigned long PATTERN_PLASMA_FRAME_INTERVAL_MS = LF_FACE_VIEW_FRAME_INTERVAL_MS;
constexpr uint8_t VIEW_TRANSITION_FADE_STEPS = 8;

static bool initializeVideoStorage()
{
  if (FFat.begin(true))
  {
    if (!FFat.exists("/videos"))
    {
      FFat.mkdir("/videos");
    }

    Serial.printf("FFat mounted: total=%lu used=%lu free=%lu\n",
                  static_cast<unsigned long>(FFat.totalBytes()),
                  static_cast<unsigned long>(FFat.usedBytes()),
                  static_cast<unsigned long>(FFat.freeBytes()));
    return true;
  }

  Serial.println("FFat mount failed; video playback disabled until storage is available.");
  return false;
}

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
#if DEBUG_MODE
volatile uint32_t gLastFrameDurationMicros = 0; // Last full frame render time
volatile uint32_t gWorstFrameDurationMicros = 0;
#endif
// ---

bool brightnessChanged = false;

constexpr unsigned long LUX_UPDATE_INTERVAL_MS = 500;
constexpr unsigned long SLEEP_CHECK_INTERVAL_MS = 60;
constexpr unsigned long BUTTON_DEBOUNCE_MS = 50;
constexpr unsigned long PAIRING_MODE_HOLD_MS = 3000;
constexpr unsigned long PAIRING_MODE_WINDOW_MS = 60000;
constexpr unsigned long PAIRING_RESET_HOLD_MS = 3000;
constexpr unsigned long BLE_STATUS_LED_INTERVAL_MS = 25;
constexpr unsigned long PROX_LUX_READ_GUARD_MS = 20;
constexpr unsigned long ANIMATION_UPDATE_INTERVAL_MS = 8;
constexpr unsigned long MOTION_CHECK_INTERVAL_MS = MOTION_SAMPLE_INTERVAL_FAST;
constexpr unsigned long BRIGHTNESS_UPDATE_INTERVAL_MS = 20;

static unsigned long lastLuxUpdateTime = 0;
static unsigned long lastSleepCheckTime = 0;
static unsigned long lastBleStatusLedUpdateTime = 0;
static unsigned long lastAnimationUpdateTime = 0;
static unsigned long lastMotionCheckTime = 0;
static unsigned long lastBrightnessUpdateTime = 0;

namespace
{
struct ButtonState
{
  bool rawPressed = false;
  bool stablePressed = false;
  bool longPressHandled = false;
  bool shortPressPending = false;
  unsigned long lastChangeMs = 0;
  unsigned long pressedAtMs = 0;
};

void updateButtonState(ButtonState &button, bool pressedSample, unsigned long nowMs, unsigned long debounceMs)
{
  if (pressedSample != button.rawPressed)
  {
    button.rawPressed = pressedSample;
    button.lastChangeMs = nowMs;
  }

  if ((nowMs - button.lastChangeMs) < debounceMs || button.stablePressed == button.rawPressed)
  {
    return;
  }

  button.stablePressed = button.rawPressed;
  if (button.stablePressed)
  {
    button.pressedAtMs = nowMs;
    button.longPressHandled = false;
    button.shortPressPending = false;
    return;
  }

  if (!button.longPressHandled)
  {
    button.shortPressPending = true;
  }
}

void suppressButtonPress(ButtonState &button)
{
  button.shortPressPending = false;
  if (button.stablePressed)
  {
    button.longPressHandled = true;
  }
}

bool consumeButtonShortPress(ButtonState &button)
{
  const bool pending = button.shortPressPending;
  button.shortPressPending = false;
  return pending;
}

bool consumeButtonLongPress(ButtonState &button, unsigned long nowMs, unsigned long holdMs)
{
  if (!button.stablePressed || button.longPressHandled)
  {
    return false;
  }

  if ((nowMs - button.pressedAtMs) < holdMs)
  {
    return false;
  }

  button.longPressHandled = true;
  button.shortPressPending = false;
  return true;
}

// Derived from the Alt_ bitmap exports with the source XBM bit order/background normalized.
static const uint8_t AltFaceNose[] PROGMEM = {
    0xb0, 0xf8, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80};
static const uint8_t AltFaceBlushL[] PROGMEM = {
    0x80, 0x8c, 0x00, 0x9a, 0xc0, 0xb6, 0x80, 0xec, 0x00, 0x80};
static const uint8_t AltFaceBrowL[] PROGMEM = {
    0xc0, 0x80, 0xc0, 0x86, 0x80, 0x8e, 0x00, 0x9e, 0x00, 0xbe, 0x00, 0xfc, 0x00, 0xf0};
static const uint8_t AltFaceScleraL[] PROGMEM = {
    0x7f, 0x00, 0x80, 0xff, 0x01, 0x80, 0xff, 0x0f, 0x80, 0xff, 0x3f, 0x80, 0xff, 0xff, 0x80, 0xfe,
    0xff, 0x82, 0xfc, 0xff, 0x86, 0xfc, 0xff, 0x8e, 0xf8, 0xff, 0x9e, 0xf0, 0xff, 0xfe, 0xe0, 0xff,
    0xbe, 0xc0, 0xff, 0x86, 0x80, 0x3f, 0x80};
static const uint8_t AltFaceBlushR[] PROGMEM = {
    0xd8, 0x80, 0x6c, 0x80, 0xb6, 0x80, 0xdb, 0x80, 0x40, 0x80};
static const uint8_t AltFaceBrowR[] PROGMEM = {
    0xc0, 0x80, 0xf0, 0x80, 0xf8, 0x80, 0x7c, 0x80, 0x3e, 0x80, 0x1f, 0x80, 0x07, 0x80};
static const uint8_t AltFaceScleraR[] PROGMEM = {
    0x00, 0x00, 0xfe, 0x00, 0xc0, 0xfe, 0x00, 0xf8, 0xfe, 0x00, 0xfe, 0xfe, 0x80, 0xff, 0xfe, 0xe0,
    0xff, 0xbe, 0xf0, 0xff, 0x9e, 0xf8, 0xff, 0x9e, 0xfc, 0xff, 0x8e, 0xff, 0xff, 0x86, 0xfe, 0xff,
    0x82, 0xf0, 0xff, 0x80, 0x00, 0xfe, 0x80};
static const uint8_t AltFacePupil[] PROGMEM = {
    0xe0, 0xe0, 0xe0, 0xe0, 0xe0, 0xe0, 0xe0, 0xe0, 0xe0, 0xe0, 0xe0};
} // namespace

// View switching
volatile uint8_t currentView = VIEW_FLAME_EFFECT; // Current view (volatile so the display task sees updates)
const int totalViews = TOTAL_VIEWS;               // Total number of views is now calculated automatically
// int userBrightness = 20; // Default brightness level (0-255)

// Plasma animation optimization
uint16_t plasmaFrameDelay = 15; // ms between plasma updates (was 10)
// Spiral animation optimization
unsigned long rotationFrameInterval = 15; // ms between spiral rotation updates (was 11)

using animation::AnimationState;
using animation::BlinkTimings;
using animation::BlushState;

AnimationState &gAnimationState = animation::state();

BlinkTimings &blinkState = gAnimationState.blinkTimings;
unsigned long &lastEyeBlinkTime = gAnimationState.lastEyeBlinkTime;
unsigned long &nextBlinkDelay = gAnimationState.nextBlinkDelay;
int &blinkProgress = gAnimationState.blinkProgress;
bool &isBlinking = gAnimationState.isBlinking;
bool &manualBlinkTrigger = gAnimationState.manualBlinkTrigger;
bool &isEyeBouncing = gAnimationState.isEyeBouncing;
unsigned long &eyeBounceStartTime = gAnimationState.eyeBounceStartTime;
volatile int &currentEyeYOffset = gAnimationState.currentEyeYOffset;
int &eyeBounceCount = gAnimationState.eyeBounceCount;
uint8_t &viewBeforeEyeBounce = gAnimationState.viewBeforeEyeBounce;
volatile int &idleEyeYOffset = gAnimationState.idleEyeYOffset;
volatile int &idleEyeXOffset = gAnimationState.idleEyeXOffset;
unsigned long &spiralStartMillis = gAnimationState.spiralStartMillis;
int &previousView = gAnimationState.previousView;
int &currentMaw = gAnimationState.currentMaw;
unsigned long &mawChangeTime = gAnimationState.mawChangeTime;
bool &mawTemporaryChange = gAnimationState.mawTemporaryChange;
bool &mouthOpen = gAnimationState.mouthOpen;
unsigned long &lastMouthTriggerTime = gAnimationState.lastMouthTriggerTime;
volatile BlushState &blushState = gAnimationState.blushState;
volatile unsigned long &blushStateStartTime = gAnimationState.blushStateStartTime;
bool &isBlushFadingIn = gAnimationState.isBlushFadingIn;
uint8_t &originalViewBeforeBlush = gAnimationState.originalViewBeforeBlush;
bool &wasBlushOverlay = gAnimationState.wasBlushOverlay;
volatile uint8_t &blushBrightness = gAnimationState.blushBrightness;

float globalBrightnessScale = 0.0f;
uint16_t globalBrightnessScaleFixed = 256;
bool facePlasmaDirty = true;
static portMUX_TYPE gLastViewPersistMux = portMUX_INITIALIZER_UNLOCKED;
static volatile bool gLastViewPersistDirty = false;
static volatile uint8_t gPendingLastView = 0;
static volatile unsigned long gPendingLastViewChangedMs = 0;
static uint8_t gLastPersistedView = 0;
static unsigned long gLastButtonInteractionMs = 0;

static void requestDisplayRefresh()
{
  if (displayTaskHandle)
  {
    xTaskNotifyGive(displayTaskHandle);
  }
}

static void noteButtonInteraction(unsigned long nowMs)
{
  gLastButtonInteractionMs = nowMs;
}

static bool shakeSuppressedByRecentButton(unsigned long nowMs)
{
  return !hasElapsedSince(nowMs, gLastButtonInteractionMs, LF_SHAKE_SUPPRESS_AFTER_BUTTON_MS);
}

static void scheduleLastViewPersist(uint8_t view)
{
  const unsigned long nowMs = millis();
  portENTER_CRITICAL(&gLastViewPersistMux);
  gPendingLastView = view;
  gPendingLastViewChangedMs = nowMs;
  gLastViewPersistDirty = true;
  portEXIT_CRITICAL(&gLastViewPersistMux);
}

static void flushPendingLastViewPersist(unsigned long nowMs)
{
  bool dirty = false;
  uint8_t pendingView = 0;
  unsigned long changedMs = 0;

  portENTER_CRITICAL(&gLastViewPersistMux);
  dirty = gLastViewPersistDirty;
  pendingView = gPendingLastView;
  changedMs = gPendingLastViewChangedMs;
  portEXIT_CRITICAL(&gLastViewPersistMux);

  if (!dirty || !hasElapsedSince(nowMs, changedMs, LF_LAST_VIEW_PERSIST_DELAY_MS))
  {
    return;
  }

  if (pendingView != gLastPersistedView)
  {
    saveLastView(pendingView);
    gLastPersistedView = pendingView;
  }

  portENTER_CRITICAL(&gLastViewPersistMux);
  if (gLastViewPersistDirty &&
      gPendingLastView == pendingView &&
      gPendingLastViewChangedMs == changedMs)
  {
    gLastViewPersistDirty = false;
  }
  portEXIT_CRITICAL(&gLastViewPersistMux);
}

void updateGlobalBrightnessScale(uint8_t brightness)
{
  globalBrightnessScale = brightness / 255.0f;
  globalBrightnessScaleFixed = static_cast<uint16_t>((static_cast<uint32_t>(brightness) * 256u + 127u) / 255u);
  facePlasmaDirty = true;
  requestDisplayRefresh();
}

struct StaticColorState
{
  CRGB color = CRGB::Black;
  bool hasValue = false;
};

static StaticColorState gStaticColorState;

static inline void drawPixelRgbFast(int x, int y, uint8_t r, uint8_t g, uint8_t b)
{
#ifdef VIRTUAL_PANE
  dma_display->drawPixel(x, y, dma_display->color565(r, g, b));
#else
  dma_display->drawPixelRGB888(x, y, r, g, b);
#endif
}

struct ScaledPlasmaPaletteCache
{
  bool valid = false;
  uint16_t brightnessScale = 0;
  uint32_t stamp = 0;
  CRGBPalette16 palette;
  CRGB entries[256];
};

static ScaledPlasmaPaletteCache gScaledPlasmaPaletteCaches[2];
static uint32_t gScaledPlasmaPaletteCacheStamp = 0;

static const CRGB *getScaledPlasmaPaletteLut(uint16_t brightnessScale)
{
  ++gScaledPlasmaPaletteCacheStamp;
  ScaledPlasmaPaletteCache *selected = &gScaledPlasmaPaletteCaches[0];

  for (ScaledPlasmaPaletteCache &cache : gScaledPlasmaPaletteCaches)
  {
    if (cache.valid &&
        cache.brightnessScale == brightnessScale &&
        std::memcmp(&cache.palette, &currentPalette, sizeof(CRGBPalette16)) == 0)
    {
      cache.stamp = gScaledPlasmaPaletteCacheStamp;
      return cache.entries;
    }

    if (!cache.valid || cache.stamp < selected->stamp)
    {
      selected = &cache;
    }
  }

  selected->valid = true;
  selected->brightnessScale = brightnessScale;
  selected->palette = currentPalette;
  selected->stamp = gScaledPlasmaPaletteCacheStamp;

  for (int i = 0; i < 256; ++i)
  {
    CRGB color = ColorFromPalette(currentPalette, static_cast<uint8_t>(i));
    color.r = static_cast<uint8_t>((static_cast<uint16_t>(color.r) * brightnessScale + 128) >> 8);
    color.g = static_cast<uint8_t>((static_cast<uint16_t>(color.g) * brightnessScale + 128) >> 8);
    color.b = static_cast<uint8_t>((static_cast<uint16_t>(color.b) * brightnessScale + 128) >> 8);
    selected->entries[i] = color;
  }

  return selected->entries;
}

static CRGB toCRGB(const RgbColor &color)
{
  return CRGB(color.r, color.g, color.b);
}

static RgbColor toRgbColor(const CRGB &color)
{
  RgbColor result;
  result.r = color.r;
  result.g = color.g;
  result.b = color.b;
  return result;
}

static bool parseColorPayloadToCRGB(const uint8_t *data, size_t length, CRGB &colorOut, std::string &normalizedHex)
{
  RgbColor parsed;
  if (!parseColorPayload(data, length, parsed, normalizedHex))
  {
    return false;
  }
  colorOut = toCRGB(parsed);
  return true;
}

static void setStaticColor(const CRGB &color)
{
  gStaticColorState.color = color;
  gStaticColorState.hasValue = true;
  constantColor = color;
  requestDisplayRefresh();
}

static void setStaticColorToDefault()
{
  setStaticColor(constantColor);
}

static bool applyStaticColorBytes(const uint8_t *data, size_t length, bool persistPreference, std::string *normalizedOut)
{
  std::string normalized;
  CRGB parsedColor;
  if (!parseColorPayloadToCRGB(data, length, parsedColor, normalized))
  {
    return false;
  }

  setStaticColor(parsedColor);
  if (persistPreference)
  {
    saveSelectedColor(String(normalized.c_str()));
  }
  if (normalizedOut)
  {
    *normalizedOut = normalized;
  }
  return true;
}

static bool loadStaticColorFromPreferences()
{
  String stored = getSelectedColor();
  if (stored.length() == 0)
  {
    setStaticColorToDefault();
    return false;
  }

  std::string rawValue;
  rawValue.reserve(stored.length());
  for (unsigned int i = 0; i < stored.length(); ++i)
  {
    rawValue.push_back(static_cast<char>(stored[i]));
  }

  if (!applyStaticColorBytes(reinterpret_cast<const uint8_t *>(rawValue.data()), rawValue.size(), false, nullptr))
  {
    setStaticColorToDefault();
    return false;
  }
  return true;
}

void ensureStaticColorLoaded()
{
  if (!gStaticColorState.hasValue)
  {
    loadStaticColorFromPreferences();
  }
}

CRGB getStaticColorCached()
{
  ensureStaticColorLoaded();
  return gStaticColorState.color;
}

static String getStaticColorHexString()
{
  ensureStaticColorLoaded();
  const std::string hex = colorToHexString(toRgbColor(gStaticColorState.color));
  return String(hex.c_str());
}

inline void setBlinkProgress(int newValue)
{
  if (blinkProgress != newValue)
  {
    facePlasmaDirty = true;
  }
  blinkProgress = newValue;
}

constexpr int MIN_BLINK_DURATION = 300;
constexpr int MAX_BLINK_DURATION = 800;
constexpr int MIN_BLINK_DELAY = 250;
constexpr int MAX_BLINK_DELAY = 5000;

constexpr unsigned long EYE_BOUNCE_DURATION = 400;
constexpr int EYE_BOUNCE_AMPLITUDE = 5;
constexpr int MAX_EYE_BOUNCES = 10;

const int IDLE_HOVER_AMPLITUDE_Y = 2.5;
const int IDLE_HOVER_AMPLITUDE_X = 2.5;
const unsigned long IDLE_HOVER_PERIOD_MS_Y = 3000;
const unsigned long IDLE_HOVER_PERIOD_MS_X = 4200;

const int totalMaws = 2;
const unsigned long mouthOpenHoldTime = 300;

const int minBlinkDuration = MIN_BLINK_DURATION;
const int maxBlinkDuration = MAX_BLINK_DURATION;
const int minBlinkDelay = MIN_BLINK_DELAY;
const int maxBlinkDelay = MAX_BLINK_DELAY;

int loadingProgress = 0;
int loadingMax = 100;

template <typename Callback>
inline void runIfElapsed(unsigned long now, unsigned long &last, unsigned long interval, Callback &&callback)
{
  if (hasElapsedSince(now, last, interval))
  {
    last = now;
    callback();
  }
}

constexpr TickType_t millisToTicksCeil(unsigned long milliseconds)
{
  return static_cast<TickType_t>((milliseconds + portTICK_PERIOD_MS - 1UL) / portTICK_PERIOD_MS);
}

char txt[64];
constexpr uint16_t SCROLL_BACKGROUND_INTERVAL_MS = 90;
constexpr int16_t SCROLL_TEXT_GAP = 12;

// Global constants for each sensitivity level
const float SLEEP_THRESHOLD = 1.0; // for sleep mode detection
const float SHAKE_THRESHOLD = 1.0; // for shake detection
// Global flag to control which threshold to use
bool useShakeSensitivity = true;

// Blush effect variables
const unsigned long fadeInDuration = 2000; // Duration for fade-in (2 seconds)
const unsigned long fullDuration = 6000;   // Full brightness time after fade-in (6 seconds)
// Total time from trigger to start fade-out is fadeInDuration + fullDuration = 8 seconds.
const unsigned long fadeOutDuration = 2000; // Duration for fade-out (2 seconds)

// Non-blocking sensor read interval
unsigned long lastSensorReadTime = 0;
const unsigned long sensorInterval = 250; // sensor read throttled to reduce I2C pressure
constexpr uint8_t PROX_TRIGGER_DELTA = 6; // How far above baseline to trigger
constexpr uint8_t PROX_RELEASE_DELTA = 2; // How close to baseline to release
constexpr unsigned long PROX_LATCH_TIMEOUT_MS = 1200;
constexpr float PROX_BASELINE_ALPHA = 0.05f; // Baseline smoothing factor
static bool proximityLatchedHigh = false;    // Prevents retriggering until sensor settles
static unsigned long proximityLatchedAt = 0;
static float proximityBaseline = 0.0f;
static bool proximityBaselineValid = false;

// Variables for plasma effect
uint16_t time_counter = 0, cycles = 0, fps = 0;
unsigned long fps_timer;

// Sleep mode variables
// Define both timeout values and select the appropriate one in setup()
const unsigned long SLEEP_TIMEOUT_MS_DEBUG = 600000; // 15 seconds (15,000 ms)
const unsigned long SLEEP_TIMEOUT_MS_NORMAL = 600000;  // 10 minutes (300,000 ms)
unsigned long SLEEP_TIMEOUT_MS;                      // Will be set in setup() based on debugMode
bool sleepModeActive = false;
unsigned long lastActivityTime = 0;
float prevAccelX = 0, prevAccelY = 0, prevAccelZ = 0;
static float lastAccelDeltaX = 0, lastAccelDeltaY = 0, lastAccelDeltaZ = 0;
static bool accelSampleValid = false;
static bool lastMotionAboveShake = false;
static bool lastMotionAboveSleep = false;
uint8_t preSleepView = VIEW_NORMAL_FACE;   // Store the view before sleep
uint8_t sleepBrightness = 15;              // Brightness level during sleep (0-255)
uint8_t normalBrightness = userBrightness; // Normal brightness level
volatile bool notifyPending = false;
unsigned long sleepFrameInterval = 50; // Frame interval in ms (will be changed during sleep)
unsigned long bluetoothStatusChangeMillis = 0;

// FPS Calculation (moved out of drawing function)
volatile int currentFPS = 0; // Use volatile if accessed by ISR, though not needed here
unsigned long lastFpsCalcTime = 0;
int frameCounter = 0;

// Full Screen Spinning Spiral Effect Variables (NEW)
float fullScreenSpiralAngle = 0.0f;
uint8_t fullScreenSpiralColorOffset = 0;
unsigned long lastFullScreenSpiralUpdateTime = 0;
// const unsigned long FULL_SCREEN_SPIRAL_FRAME_INTERVAL_MS = 50; // Adjusted for performance
const unsigned long FULL_SCREEN_SPIRAL_FRAME_INTERVAL_MS = 0; // Further adjusted if needed

const float FULL_SPIRAL_ROTATION_SPEED_RAD_PER_UPDATE = 0.2f;
const uint8_t FULL_SPIRAL_COLOR_SCROLL_SPEED = 2.5;

// Logarithmic Spiral Coefficients (NEW/MODIFIED)
const float LOG_SPIRAL_A_COEFF = 4.0f;  // Initial radius factor (when theta is small or exp(0))
                                        // Smaller 'a' means it starts smaller/tighter from center.
const float LOG_SPIRAL_B_COEFF = 0.10f; // Controls how tightly wound the spiral is.
                                        // Smaller 'b' makes it wind more slowly (appearing tighter initially for a given theta range).
                                        // Larger 'b' makes it expand much faster.
                                        // Positive 'b' for outward spiral, negative for inward.

const float SPIRAL_ARM_COLOR_FACTOR = 5.0f; // Could be based on 'r' now instead of 'theta_arm'
const float SPIRAL_THICKNESS_RADIUS = 1.0f; // Start with 0 for performance, then increase

static portMUX_TYPE gBleWorkQueueStatsMux = portMUX_INITIALIZER_UNLOCKED;
static BleWorkQueueStats gBleWorkQueueStats;

static void bleWorkerTask(void *param);
static void handleBleCommandWork(const BleWorkItem &item);
static void handleBleFaceWriteWork(const BleWorkItem &item);
static void handleBleConfigWriteWork(const BleWorkItem &item);
static void handleBleBrightnessWriteWork(const BleWorkItem &item);
static void handleBleStaticColorWriteWork(const BleWorkItem &item);
static void handleBleStrobeSettingsWriteWork(const BleWorkItem &item);

static void bleRecordQueueDepth(UBaseType_t depth)
{
  portENTER_CRITICAL(&gBleWorkQueueStatsMux);
  if (depth > gBleWorkQueueStats.highWaterDepth)
  {
    gBleWorkQueueStats.highWaterDepth = depth;
  }
  portEXIT_CRITICAL(&gBleWorkQueueStatsMux);
}

static void bleRecordQueueDrop()
{
  portENTER_CRITICAL(&gBleWorkQueueStatsMux);
  ++gBleWorkQueueStats.drops;
  portEXIT_CRITICAL(&gBleWorkQueueStatsMux);
}

static BleWorkQueueStats bleTakeQueueStats()
{
  BleWorkQueueStats snapshot{};
  portENTER_CRITICAL(&gBleWorkQueueStatsMux);
  snapshot = gBleWorkQueueStats;
  gBleWorkQueueStats = {};
  portEXIT_CRITICAL(&gBleWorkQueueStatsMux);
  return snapshot;
}

static bool bleQueueWorkInternal(const BleWorkItem &item)
{
  if (bleWorkQueue == nullptr)
  {
    return false;
  }

  if (xQueueSend(bleWorkQueue, &item, 0) != pdPASS)
  {
    bleRecordQueueDrop();
    return false;
  }

  bleRecordQueueDepth(uxQueueMessagesWaiting(bleWorkQueue));
  return true;
}

bool bleQueuePayload(BleWorkType type, NimBLECharacteristic *characteristic, const uint8_t *data, size_t length)
{
  if ((length > 0 && data == nullptr) || length > LF_BLE_WORK_ITEM_PAYLOAD_MAX)
  {
    return false;
  }

  BleWorkItem item{};
  item.type = type;
  item.characteristic = characteristic;
  item.length = static_cast<uint16_t>(length);
  if (length > 0)
  {
    std::memcpy(item.data, data, length);
  }
  return bleQueueWorkInternal(item);
}

bool bleQueueString(BleWorkType type, NimBLECharacteristic *characteristic, const std::string &value)
{
  return bleQueuePayload(type, characteristic,
                         reinterpret_cast<const uint8_t *>(value.data()),
                         value.size());
}

bool bleQueueByte(BleWorkType type, NimBLECharacteristic *characteristic, uint8_t value)
{
  return bleQueuePayload(type, characteristic, &value, 1);
}

bool bleQueueEmpty(BleWorkType type, NimBLECharacteristic *characteristic)
{
  return bleQueuePayload(type, characteristic, nullptr, 0);
}

bool bleIsOtaBusy()
{
  return otaCallbacks.isActive();
}

inline void notifyBleTask()
{
  if (deviceConnected && bleNotifyTaskHandle)
  {
    xTaskNotifyGive(bleNotifyTaskHandle);
  }
}

struct _InitScrollSpeed
{
  _InitScrollSpeed() { scroll::updateIntervalFromSpeed(); }
} _initScrollSpeedOnce; // Ensure scroll interval is set at startup

void calculateFPS()
{
  frameCounter++;
  unsigned long now = millis();
  if (now - lastFpsCalcTime >= 1000)
  {
    currentFPS = frameCounter;
    frameCounter = 0;
    lastFpsCalcTime = now;
    // Serial.printf("FPS: %d\n", currentFPS); // Optional: Print FPS once per second
  }
}

void maybeUpdateTemperature()
{
  if (!deviceConnected)
  {
    return;
  }

  static unsigned long lastTempUpdate = 0;
  const unsigned long tempUpdateInterval = 5000;       // 5 seconds
  const unsigned long sleepTempUpdateInterval = 10000; // 10 seconds
  const unsigned long now = millis();
  const unsigned long targetInterval = sleepModeActive ? sleepTempUpdateInterval : tempUpdateInterval;

  if (now - lastTempUpdate >= targetInterval)
  {
    updateTemperature();
    lastTempUpdate = now;
  }
}

void displayCurrentView(int view);
static bool viewUsesMic(int view)
{
  switch (view)
  {
  case VIEW_NORMAL_FACE:
  case VIEW_BLUSH_FACE:
  case VIEW_SEMICIRCLE_EYES:
  case VIEW_X_EYES:
  case VIEW_SLANT_EYES:
  case VIEW_SPIRAL_EYES:
  case VIEW_PLASMA_FACE:
  case VIEW_UWU_EYES:
  case VIEW_CIRCLE_EYES:
  case VIEW_ALT_FACE:
    return true;
  default:
    return false;
  }
}

static bool viewNeedsContinuousRefresh(int view)
{
  switch (view)
  {
  case VIEW_TRANS_FLAG:
  case VIEW_LGBT_FLAG:
  case VIEW_BSOD:
  case VIEW_STATIC_COLOR:
    return false;
  default:
    return true;
  }
}

void wakeFromSleepMode()
{
  if (!sleepModeActive)
    return; // Already awake

  Serial.println(">>> Waking from sleep mode <<<");
  sleepModeActive = false;
  currentView = preSleepView; // Restore previous view
  micSetEnabled(viewUsesMic(currentView));
  requestDisplayRefresh();

  // Restore normal CPU speed
  restoreNormalCPUSpeed();

  // Restore normal frame rate
  sleepFrameInterval = 5; // Back to ~90 FPS

  // Restore normal brightness
  dma_display->setBrightness8(userBrightness);
  updateGlobalBrightnessScale(userBrightness);
  syncBrightnessState(userBrightness);

  lastActivityTime = millis(); // Reset activity timer

  // Notify all clients if connected
  if (deviceConnected)
  {
    // Also send current view back to the app
    uint8_t bleViewValue = static_cast<uint8_t>(currentView);
    pFaceCharacteristic->setValue(&bleViewValue, 1);
    pFaceCharacteristic->notify();

    // Send a temperature update
    maybeUpdateTemperature();
  }

  // Restore normal BLE advertising if not connected
  if (!deviceConnected)
  {
    NimBLEDevice::getAdvertising()->setMinInterval(160); // 100 ms (default)
    NimBLEDevice::getAdvertising()->setMaxInterval(240); // 150 ms (default)
    refreshBleAdvertising();
  }
}

// Callback class for handling writes to the command characteristic
class CommandCallbacks : public NimBLECharacteristicCallbacks
{

  // Override the onWrite method
  void onWrite(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &connInfo) override
  {
    (void)connInfo;
    ScopedPerfTelemetryDuration perfScope(PerfDurationId::BleCallback);
    // Get the value written by the client
    const auto &rxValue = pCharacteristic->getValue();

    if (rxValue.size() > 0)
    {
      const uint8_t commandCode = rxValue[0]; // Assuming single-byte commands

      Serial.print("Command Characteristic received write, command code: 0x");
      Serial.println(commandCode, HEX);
      if (!bleQueueByte(BleWorkType::Command, pCharacteristic, commandCode))
      {
        Serial.println("BLE command queue full, dropping command.");
      }
    }
    else
    {
      Serial.println("Command Characteristic received empty write.");
    }
  }
} cmdCallbacks;

// New Callback class for handling notifications related to Temperature Logs
class TemperatureLogsCallbacks : public NimBLECharacteristicCallbacks
{
public:
  // Optionally, implement onSubscribe to log when a client subscribes to notifications
  void onSubscribe(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &connInfo, uint16_t subValue) override
  {
    std::string str = "Client subscribed to Temperature Logs notifications, Client ID: ";
    str += connInfo.getConnHandle();
    str += " Address: ";
    str += connInfo.getAddress().toString();
    Serial.printf("%s\n", str.c_str());
  }
};
TemperatureLogsCallbacks logsCallbacks;

// Class to handle characteristic callbacks
class CharacteristicCallbacks : public NimBLECharacteristicCallbacks
{
  void onRead(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &connInfo) override
  {
    (void)connInfo;
    uint8_t bleViewValue = static_cast<uint8_t>(currentView);
    pCharacteristic->setValue(&bleViewValue, 1);
    Serial.printf("Read request - returned view: %d\n", bleViewValue);
  }

  void onWrite(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &connInfo) override
  {
    (void)connInfo;
    ScopedPerfTelemetryDuration perfScope(PerfDurationId::BleCallback);
    const auto &value = pCharacteristic->getValue();
    if (value.size() < 1)
    {
      return;
    }
    const uint8_t newView = value[0];
    if (!bleQueueByte(BleWorkType::FaceWrite, pCharacteristic, newView))
    {
      Serial.println("BLE face write queue full, dropping view update.");
    }
  }

  void onStatus(NimBLECharacteristic *pCharacteristic, int code) override
  {
    Serial.printf("Notification/Indication return code: %d, %s\n", code, NimBLEUtils::returnCodeToString(code));
  }

  void onSubscribe(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &connInfo, uint16_t subValue) override
  {
    std::string str = "Client ID: ";
    str += connInfo.getConnHandle();
    str += " Address: ";
    str += connInfo.getAddress().toString();
    if (subValue == 0)
    {
      str += " Unsubscribed to ";
    }
    else if (subValue == 1)
    {
      str += " Subscribed to notifications for ";
    }
    else if (subValue == 2)
    {
      str += " Subscribed to indications for ";
    }
    else if (subValue == 3)
    {
      str += " Subscribed to notifications and indications for ";
    }
    str += std::string(pCharacteristic->getUUID());

    Serial.printf("%s\n", str.c_str());
  }
} chrCallbacks;

// Custom characteristic callback class.
class ConfigCallbacks : public NimBLECharacteristicCallbacks
{
  void onWrite(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &connInfo) override
  {
    (void)connInfo;
    ScopedPerfTelemetryDuration perfScope(PerfDurationId::BleCallback);
    const auto &value = pCharacteristic->getValue();
    if (value.size() < 4)
    {
      Serial.println("Error: Config payload is not at least 4 bytes.");
      return;
    }
    if (!bleQueuePayload(BleWorkType::ConfigWrite, pCharacteristic, value.data(), value.size()))
    {
      Serial.println("BLE config queue full, dropping configuration update.");
    }
  }
};
// Reuse one instance to avoid heap fragmentation
static ConfigCallbacks configCallbacks;

class BrightnessCallbacks : public NimBLECharacteristicCallbacks
{
  void onWrite(NimBLECharacteristic *pChr, NimBLEConnInfo &connInfo) override
  {
    (void)connInfo;
    ScopedPerfTelemetryDuration perfScope(PerfDurationId::BleCallback);
    auto val = pChr->getValue();
    if (val.size() >= 1)
    {
      if (!bleQueueByte(BleWorkType::BrightnessWrite, pChr, static_cast<uint8_t>(val[0])))
      {
        Serial.println("BLE brightness queue full, dropping brightness update.");
      }
    }
  }
};
static BrightnessCallbacks brightnessCallbacks;

class StaticColorCallbacks : public NimBLECharacteristicCallbacks
{
  void onWrite(NimBLECharacteristic *pChr, NimBLEConnInfo &connInfo) override
  {
    (void)connInfo;
    ScopedPerfTelemetryDuration perfScope(PerfDurationId::BleCallback);
    const auto &val = pChr->getValue();
    if (val.size() == 0)
    {
      return;
    }

    if (!bleQueuePayload(BleWorkType::StaticColorWrite, pChr, val.data(), val.size()))
    {
      Serial.println("BLE static color queue full, dropping payload.");
    }
  }

  void onRead(NimBLECharacteristic *pChr, NimBLEConnInfo &connInfo) override
  {
    String hexString = getStaticColorHexString();
    std::string value(hexString.c_str());
    pChr->setValue(value);
  }
};
static StaticColorCallbacks staticColorCallbacks;

static std::string buildStrobeSettingsPayload()
{
  String hexString = getStrobeColorHexString();
  return std::string("{") +
         "\"color\":\"" + std::string(hexString.c_str()) + "\"," +
         "\"speedMs\":" + std::to_string(static_cast<unsigned int>(getStrobeSpeedMs())) +
         "}";
}

class StrobeSettingsCallbacks : public NimBLECharacteristicCallbacks
{
  void onWrite(NimBLECharacteristic *pChr, NimBLEConnInfo &connInfo) override
  {
    (void)connInfo;
    ScopedPerfTelemetryDuration perfScope(PerfDurationId::BleCallback);
    const auto &val = pChr->getValue();
    if (val.size() == 0)
    {
      return;
    }
    if (!bleQueuePayload(BleWorkType::StrobeSettingsWrite, pChr, val.data(), val.size()))
    {
      Serial.println("BLE strobe queue full, dropping payload.");
    }
  }

  void onRead(NimBLECharacteristic *pChr, NimBLEConnInfo &connInfo) override
  {
    const std::string value = buildStrobeSettingsPayload();
    pChr->setValue(value);
  }
};
static StrobeSettingsCallbacks strobeSettingsCallbacks;

static void handleBleCommandWork(const BleWorkItem &item)
{
  if (item.length < 1)
  {
    return;
  }

  const uint8_t commandCode = item.data[0];
  switch (commandCode)
  {
  case 0x01:
    Serial.println("-> Command: Get Temperature History");
    triggerHistoryTransfer();
    break;
  case 0x02:
    Serial.println("-> Command: Clear History Buffer");
    clearHistoryBuffer();
    break;
  default:
    Serial.print("-> Unknown command code received: 0x");
    Serial.println(commandCode, HEX);
    break;
  }
}

static void handleBleFaceWriteWork(const BleWorkItem &item)
{
  if (item.length < 1)
  {
    return;
  }

  const uint8_t newView = item.data[0];
  if (sleepModeActive)
  {
    Serial.println("BLE write received while sleeping, waking up...");
    wakeFromSleepMode();
  }

  if (newView >= totalViews)
  {
#if defined(DEBUG_BLE)
    Serial.printf("BLE Write ignored - invalid view number: %d (Total Views: %d)\n", newView, totalViews);
#endif
    return;
  }

  lastActivityTime = millis();
  if (newView == currentView)
  {
    return;
  }

  currentView = newView;
  scheduleLastViewPersist(currentView);
  requestDisplayRefresh();
#if defined(DEBUG_BLE)
  Serial.printf("Write request - new view: %d\n", currentView);
#endif
  notifyBleTask();
}

static void handleBleConfigWriteWork(const BleWorkItem &item)
{
  if (item.length < 4)
  {
    Serial.println("Error: Config payload is not at least 4 bytes.");
    return;
  }

#if defined(DEBUG_BLE)
  Serial.println("Received configuration update:");
#endif
  const bool oldAutoBrightnessEnabled = autoBrightnessEnabled;
  const bool oldAuroraMode = auroraModeEnabled;
  const bool oldStaticColorMode = staticColorModeEnabled;

  autoBrightnessEnabled = (item.data[0] != 0);
  accelerometerEnabled = (item.data[1] != 0);
  sleepModeEnabled = (item.data[2] != 0);
  auroraModeEnabled = (item.data[3] != 0);
  if (item.length >= 5)
  {
    staticColorModeEnabled = (item.data[4] != 0);
  }
  else
  {
    staticColorModeEnabled = false;
#if defined(DEBUG_BLE)
    Serial.println("Static color flag not provided, defaulting to disabled.");
#endif
  }

  if (staticColorModeEnabled)
  {
    auroraModeEnabled = false;
  }

  setAutoBrightness(autoBrightnessEnabled);
  setAccelerometerEnabled(accelerometerEnabled);
  setSleepMode(sleepModeEnabled);
  setAuroraMode(auroraModeEnabled);
  setStaticColorMode(staticColorModeEnabled);

#if defined(DEBUG_BLE)
  Serial.print("  Auto Brightness: ");
  Serial.println(autoBrightnessEnabled ? "Enabled" : "Disabled");
  Serial.print("  Accelerometer:   ");
  Serial.println(accelerometerEnabled ? "Enabled" : "Disabled");
  Serial.print("  Sleep Mode:      ");
  Serial.println(sleepModeEnabled ? "Enabled" : "Disabled");
  Serial.print("  Aurora Mode:     ");
  Serial.println(auroraModeEnabled ? "Enabled" : "Disabled");
  Serial.print("  Static Color:    ");
  Serial.println(staticColorModeEnabled ? "Enabled" : "Disabled");
#endif

  applyConfigOptions();
  if (oldAutoBrightnessEnabled != autoBrightnessEnabled)
  {
    if (autoBrightnessEnabled)
    {
#if defined(DEBUG_BLE)
      Serial.println("Auto brightness has been ENABLED. Adaptive logic will take over.");
#endif
    }
    else
    {
#if defined(DEBUG_BLE)
      Serial.println("Auto brightness has been DISABLED. Applying user-set brightness.");
      Serial.printf("Applied manual brightness: %u\n", userBrightness);
#endif
      dma_display->setBrightness8(userBrightness);
      updateGlobalBrightnessScale(userBrightness);
      syncBrightnessState(userBrightness);
    }
  }

  if (oldAuroraMode != auroraModeEnabled || oldStaticColorMode != staticColorModeEnabled)
  {
    facePlasmaDirty = true;
    requestDisplayRefresh();
  }
}

static void handleBleBrightnessWriteWork(const BleWorkItem &item)
{
  if (item.length < 1)
  {
    return;
  }

  userBrightness = item.data[0];
  brightnessChanged = true;
  setUserBrightness(userBrightness);
#if DEBUG_BRIGHTNESS
  Serial.printf("Brightness target set to %u\n", userBrightness);
#endif
}

static void handleBleStaticColorWriteWork(const BleWorkItem &item)
{
  if (item.characteristic == nullptr || item.length == 0)
  {
    return;
  }

  std::string normalized;
  if (!applyStaticColorBytes(item.data, item.length, true, &normalized))
  {
    Serial.println("Invalid static color payload received over BLE.");
    return;
  }

  item.characteristic->setValue(normalized);
  item.characteristic->notify();
#if DEBUG_MODE
  Serial.printf("Static color updated to #%s\n", normalized.c_str());
#endif
}

static void handleBleStrobeSettingsWriteWork(const BleWorkItem &item)
{
  if (item.characteristic == nullptr || item.length < 2)
  {
    return;
  }

  constexpr uint8_t kOpcodeColor = 0x01;
  constexpr uint8_t kOpcodeSpeed = 0x02;

  const uint8_t opcode = item.data[0];
  const uint8_t *payload = item.data + 1;
  const size_t payloadLength = item.length - 1;

  if (opcode == kOpcodeColor)
  {
    std::string normalized;
    if (!applyStrobeColorBytes(payload, payloadLength, true, &normalized))
    {
      Serial.println("Invalid strobe color payload received over BLE.");
      return;
    }
#if DEBUG_MODE
    Serial.printf("Strobe color updated to #%s\n", normalized.c_str());
#endif
  }
  else if (opcode == kOpcodeSpeed)
  {
    uint16_t speedMs = 0;
    if (!applyStrobeSpeedBytes(payload, payloadLength, true, &speedMs))
    {
      Serial.println("Invalid strobe speed payload received over BLE.");
      return;
    }
#if DEBUG_MODE
    Serial.printf("Strobe speed updated to %u ms\n", static_cast<unsigned int>(speedMs));
#endif
  }
  else
  {
    Serial.printf("Invalid strobe settings opcode received over BLE: 0x%02X\n", opcode);
    return;
  }

  const std::string response = buildStrobeSettingsPayload();
  item.characteristic->setValue(response);
  item.characteristic->notify();
}

static void bleWorkerTask(void *param)
{
  (void)param;
  BleWorkItem item{};

  for (;;)
  {
    if (xQueueReceive(bleWorkQueue, &item, portMAX_DELAY) != pdPASS)
    {
      continue;
    }

    const uint32_t workStartMicros = micros();
    PERF_SCOPE(PerfBucket::Ble);
    switch (item.type)
    {
    case BleWorkType::Command:
      handleBleCommandWork(item);
      break;
    case BleWorkType::FaceWrite:
      handleBleFaceWriteWork(item);
      break;
    case BleWorkType::ConfigWrite:
      handleBleConfigWriteWork(item);
      break;
    case BleWorkType::BrightnessWrite:
      handleBleBrightnessWriteWork(item);
      break;
    case BleWorkType::StaticColorWrite:
      handleBleStaticColorWriteWork(item);
      break;
    case BleWorkType::StrobeSettingsWrite:
      handleBleStrobeSettingsWriteWork(item);
      break;
    case BleWorkType::ScrollWrite:
      handleScrollWritePayload(item.characteristic, item.data, item.length);
      break;
    case BleWorkType::OtaPacket:
      otaCallbacks.processPacket(item.characteristic, item.data, item.length);
      break;
    default:
      break;
    }
    perfTelemetryRecordDuration(PerfDurationId::BleWorkerTaskBusy, micros() - workStartMicros);
  }
}

class DescriptorCallbacks : public NimBLEDescriptorCallbacks
{
  void onWrite(NimBLEDescriptor *pDescriptor, NimBLEConnInfo &connInfo) override
  {
    std::string dscVal = pDescriptor->getValue();
    Serial.printf("Descriptor written value: %s\n", dscVal.c_str());
  }
  void onRead(NimBLEDescriptor *pDescriptor, NimBLEConnInfo &connInfo) override
  {
    Serial.printf("%s Descriptor read\n", pDescriptor->getUUID().toString().c_str());
  }
} dscCallbacks;

void handleBLEStatusLED()
{
  const PairingSnapshot pairingSnapshot = getPairingSnapshot();
  static bool lastPairingActive = false;
  static bool lastPasskeyValid = false;
  static uint32_t lastPasskey = 0;
  bool pixelShown = false;
  if (deviceConnected != oldDeviceConnected)
  {
    bluetoothStatusChangeMillis = millis();
    requestDisplayRefresh();
    if (deviceConnected)
    {
      statusPixel.setPixelColor(0, 0, 50, 0); // Green when connected
    }
    else
    {
      statusPixel.setPixelColor(0, 0, 0, 0); // Off when disconnected
      refreshBleAdvertising();
    }
    // statusPixel.show();
    oldDeviceConnected = deviceConnected;
  }
  if (pairingSnapshot.pairing != lastPairingActive ||
      pairingSnapshot.passkeyValid != lastPasskeyValid ||
      pairingSnapshot.passkey != lastPasskey)
  {
    requestDisplayRefresh();
    lastPairingActive = pairingSnapshot.pairing;
    lastPasskeyValid = pairingSnapshot.passkeyValid;
    lastPasskey = pairingSnapshot.passkey;
  }
#if DEBUG_DISABLE_BLE_INDICATOR_LIGHT
  statusPixel.setPixelColor(0, 0, 0, 0);
  statusPixel.show();
  return;
#endif
  if (pairingSnapshot.pairing)
  {
    fadeInAndOutLED(128, 0, 128); // Purple fade when pairing
    pixelShown = true;
  }
  else if (shouldBleAdvertise())
  {
    fadeInAndOutLED(0, 0, 100); // Blue fade when disconnected
    pixelShown = true;
  }

  if (!pixelShown)
  {
    statusPixel.show();
  }
}

static void resetBlePairing()
{
  if (!pServer)
  {
    return;
  }

  startPairingMode(PAIRING_MODE_WINDOW_MS);
  const std::vector<uint16_t> peers = pServer->getPeerDevices();
  if (!peers.empty())
  {
    setPairingResetPending(true);
    for (uint16_t connHandle : peers)
    {
      pServer->disconnect(connHandle);
    }
    return;
  }

  const bool cleared = NimBLEDevice::deleteAllBonds();
  setPairingResetPending(false);
  refreshBleAdvertising();
#if defined(DEBUG_BLE)
  Serial.printf("BLE pairing reset: bonds cleared=%s\n", cleared ? "true" : "false");
#endif
}

// Bitmap Drawing Functions ------------------------------------------------
void drawXbm565(int x, int y, int width, int height, const uint8_t *xbm, uint16_t color = 0xffff)
{
  // Ensure width is padded to the nearest byte boundary
  int byteWidth = (width + 7) / 8;

  // Pre-check if entire bitmap is out of bounds
  if (x >= dma_display->width() || y >= dma_display->height() ||
      x + width <= 0 || y + height <= 0)
  {
    return; // Completely out of bounds, nothing to draw
  }

  // Calculate visible region to avoid per-pixel boundary checks
  // Use explicit conditionals instead of min/max to avoid overload or macro conflicts.
  int startX = (0 > -x) ? 0 : -x;
  int startY = (0 > -y) ? 0 : -y;
  int tmpEndX = dma_display->width() - x;
  int endX = (width < tmpEndX) ? width : tmpEndX;
  int tmpEndY = dma_display->height() - y;
  int endY = (height < tmpEndY) ? height : tmpEndY;

  for (int j = startY; j < endY; j++)
  {
    uint8_t bitMask = 0x80 >> (startX & 7);        // Start with the correct bit position
    int byteIndex = j * byteWidth + (startX >> 3); // Integer division by 8

    for (int i = startX; i < endX; i++)
    {
      // Check if the bit is set
      if (pgm_read_byte(&xbm[byteIndex]) & bitMask)
      {
        dma_display->drawPixel(x + i, y + j, color);
      }

      // Move to the next bit
      bitMask >>= 1;
      if (bitMask == 0)
      {                 // We've used all bits in this byte
        bitMask = 0x80; // Reset to the first bit of the next byte
        byteIndex++;    // Move to the next byte
      }
    }
  }
}

constexpr uint8_t BLUETOOTH_BACKGROUND_WIDTH = 7;
constexpr uint8_t BLUETOOTH_BACKGROUND_HEIGHT = 11;
constexpr uint8_t BLUETOOTH_RUNE_WIDTH = 5;
constexpr uint8_t BLUETOOTH_RUNE_HEIGHT = 7;
constexpr uint8_t BLUETOOTH_ICON_MARGIN = 1;
constexpr unsigned long BLUETOOTH_FADE_PERIOD_MS = 1600UL;
constexpr unsigned long BLUETOOTH_CONNECTED_FADE_DELAY_MS = 2000UL;
constexpr unsigned long BLUETOOTH_CONNECTED_FADE_DURATION_MS = 1500UL;

static bool bluetoothOverlayNeedsContinuousRefresh(unsigned long nowMs)
{
#if DEBUG_DISABLE_BLE_STATUS_ICON
  (void)nowMs;
  return false;
#else
  if (!deviceConnected)
  {
    return true;
  }

  if (bluetoothStatusChangeMillis == 0)
  {
    return false;
  }

  const unsigned long connectedDuration = nowMs - bluetoothStatusChangeMillis;
  return connectedDuration <= (BLUETOOTH_CONNECTED_FADE_DELAY_MS + BLUETOOTH_CONNECTED_FADE_DURATION_MS);
#endif
}

static unsigned long viewFrameIntervalMillis(int view)
{
  switch (view)
  {
  case VIEW_PATTERN_PLASMA:
  case VIEW_NORMAL_FACE:
  case VIEW_BLUSH_FACE:
  case VIEW_SEMICIRCLE_EYES:
  case VIEW_X_EYES:
  case VIEW_SLANT_EYES:
  case VIEW_SPIRAL_EYES:
  case VIEW_PLASMA_FACE:
  case VIEW_UWU_EYES:
  case VIEW_CIRCLE_EYES:
  case VIEW_ALT_FACE:
    return PATTERN_PLASMA_FRAME_INTERVAL_MS;
  case VIEW_DVD_LOGO:
    return dvdUpdateInterval;
  default:
    return targetFrameIntervalMillis;
  }
}

static uint32_t viewFrameBudgetMicros(int view)
{
  unsigned long frameIntervalMs = viewFrameIntervalMillis(view);
  if (bleIsOtaBusy() && frameIntervalMs < LF_OTA_ACTIVE_FRAME_INTERVAL_MS)
  {
    frameIntervalMs = LF_OTA_ACTIVE_FRAME_INTERVAL_MS;
  }
  return static_cast<uint32_t>(frameIntervalMs) * 1000UL;
}

#if PERF_SELF_TEST
static void runPerfSelfTest()
{
  bool passed = true;

  if (targetFrameIntervalMillis == 0)
  {
    Serial.println("SELFTEST: targetFrameIntervalMillis must be > 0");
    passed = false;
  }

  if (PROX_RELEASE_DELTA > PROX_TRIGGER_DELTA)
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
#endif

static uint8_t scaleColorComponent(uint8_t value, float intensity)
{
  float scaled = value * intensity;
  if (scaled < 0.0f)
  {
    scaled = 0.0f;
  }
  if (scaled > 255.0f)
  {
    scaled = 255.0f;
  }
  return static_cast<uint8_t>(scaled + 0.5f);
}

void drawBluetoothStatusIcon()
{
#if DEBUG_DISABLE_BLE_STATUS_ICON
  return;
#endif
  if (!dma_display)
  {
    return;
  }

  const unsigned long nowMs = millis();
  const int iconX = dma_display->width() - BLUETOOTH_BACKGROUND_WIDTH - BLUETOOTH_ICON_MARGIN - 64;
  const int iconY = dma_display->height() - BLUETOOTH_BACKGROUND_HEIGHT - BLUETOOTH_ICON_MARGIN;

  float intensity = 1.0f;
  if (!deviceConnected)
  {
    const float normalized = static_cast<float>(nowMs % BLUETOOTH_FADE_PERIOD_MS) / static_cast<float>(BLUETOOTH_FADE_PERIOD_MS);
    const float wave = 0.5f * (sinf(normalized * TWO_PI) + 1.0f); // Range 0..1
    intensity = 0.15f + (0.65f * wave);                           // Keep a faint glow at minimum
  }
  else
  {
    if (bluetoothStatusChangeMillis == 0)
    {
      bluetoothStatusChangeMillis = nowMs;
    }
    const unsigned long connectedDuration = nowMs - bluetoothStatusChangeMillis;
    if (connectedDuration > BLUETOOTH_CONNECTED_FADE_DELAY_MS)
    {
      const unsigned long fadeElapsed = connectedDuration - BLUETOOTH_CONNECTED_FADE_DELAY_MS;
      if (fadeElapsed >= BLUETOOTH_CONNECTED_FADE_DURATION_MS)
      {
        intensity = 0.0f;
      }
      else
      {
        const float fadeProgress = static_cast<float>(fadeElapsed) / static_cast<float>(BLUETOOTH_CONNECTED_FADE_DURATION_MS);
        intensity = 1.0f - fadeProgress;
      }
    }
  }

  // After fade-out completes, stop drawing the connected icon in normal mode.
  if (deviceConnected && !sleepModeActive && intensity <= 0.0f)
  {
    return;
  }

  const uint16_t backgroundColor = dma_display->color565(
      scaleColorComponent(5, intensity),
      scaleColorComponent(90, intensity),
      scaleColorComponent(180, intensity));

  // NEW: Brighter constant for ble pixel color
  const uint16_t blePixelColor = dma_display->color565(
      scaleColorComponent(5, intensity + 0.8f),
      scaleColorComponent(90, intensity + 0.8f),
      scaleColorComponent(180, intensity + 0.8f));

  const uint16_t runeColor = dma_display->color565(
      scaleColorComponent(255, intensity),
      scaleColorComponent(255, intensity),
      scaleColorComponent(255, intensity));

  // Clear the drawing area (pad by 1 px to avoid leftover pixels from other views)
  int clearX = iconX - 1;
  int clearY = iconY - 1;
  int clearW = BLUETOOTH_BACKGROUND_WIDTH + 2;
  int clearH = BLUETOOTH_BACKGROUND_HEIGHT + 2;
  if (clearX < 0)
  {
    clearW += clearX;
    clearX = 0;
  }
  if (clearY < 0)
  {
    clearH += clearY;
    clearY = 0;
  }
  if (clearW > 0 && clearH > 0)
  {
    // dma_display->fillRect(clearX, clearY, clearW, clearH, 0);
  }
  if (!sleepModeActive)
  {
    drawXbm565(iconX, iconY, BLUETOOTH_BACKGROUND_WIDTH, BLUETOOTH_BACKGROUND_HEIGHT, bluetoothBackground, backgroundColor);
    drawXbm565(iconX + BLUETOOTH_BACKGROUND_WIDTH + 2, iconY, BLUETOOTH_BACKGROUND_WIDTH, BLUETOOTH_BACKGROUND_HEIGHT, bluetoothBackground, backgroundColor);

    const int runeX = iconX + static_cast<int>((BLUETOOTH_BACKGROUND_WIDTH - BLUETOOTH_RUNE_WIDTH) / 2);
    const int runeY = iconY + static_cast<int>((BLUETOOTH_BACKGROUND_HEIGHT - BLUETOOTH_RUNE_HEIGHT) / 2);

    drawXbm565(runeX, runeY, BLUETOOTH_RUNE_WIDTH, BLUETOOTH_RUNE_HEIGHT, bluetoothRune, runeColor);
    drawXbm565(runeX + BLUETOOTH_RUNE_WIDTH + 4, runeY, BLUETOOTH_RUNE_WIDTH, BLUETOOTH_RUNE_HEIGHT, bluetoothRune, runeColor);
  }
  else
  {
    dma_display->drawPixel(63, 0, blePixelColor);
    dma_display->drawPixel(64, 0, blePixelColor);
  }
}

static void drawPairingPasskeyOverlay(uint32_t passkey)
{
  if (!dma_display)
  {
    return;
  }

  static bool labelMetricsCached = false;
  static int16_t labelX1 = 0;
  static int16_t labelY1 = 0;
  static uint16_t labelW = 0;
  static uint16_t labelH = 0;
  static bool keyMetricsCached = false;
  static uint32_t cachedPasskey = 0;
  static int16_t keyX1 = 0;
  static int16_t keyY1 = 0;
  static uint16_t keyW = 0;
  static uint16_t keyH = 0;

  char passkeyStr[7];
  snprintf(passkeyStr, sizeof(passkeyStr), "%06lu", static_cast<unsigned long>(passkey));

  const char *label = "PASSKEY";

  if (!labelMetricsCached)
  {
    dma_display->setFont(&TomThumb);
    dma_display->setTextSize(1);
    dma_display->getTextBounds(label, 0, 0, &labelX1, &labelY1, &labelW, &labelH);
    labelMetricsCached = true;
  }

  if (!keyMetricsCached || passkey != cachedPasskey)
  {
    dma_display->setFont(&FreeSans9pt7b);
    dma_display->setTextSize(1);
    dma_display->getTextBounds(passkeyStr, 0, 0, &keyX1, &keyY1, &keyW, &keyH);
    cachedPasskey = passkey;
    keyMetricsCached = true;
  }

  const int padding = 2;
  const int gap = 1;
  const int contentW = (labelW > keyW) ? labelW : keyW;
  const int boxW = contentW + (padding * 2);
  const int boxH = labelH + keyH + gap + (padding * 2);

  const int screenW = dma_display->width();
  const int screenH = dma_display->height();
  int boxX = (screenW - boxW) / 2;
  int boxY = (screenH - boxH) / 2;

  if (boxX < 0)
  {
    boxX = 0;
  }
  if (boxY < 0)
  {
    boxY = 0;
  }

  const uint16_t backgroundColor = dma_display->color565(0, 0, 0);
  const uint16_t borderColor = dma_display->color565(255, 255, 255);

  dma_display->fillRect(boxX, boxY, boxW, boxH, backgroundColor);
  dma_display->drawRect(boxX, boxY, boxW, boxH, borderColor);

  const int labelLeft = boxX + (boxW - labelW) / 2;
  const int labelTop = boxY + padding;
  const int labelCursorX = labelLeft - labelX1;
  const int labelCursorY = labelTop - labelY1;

  dma_display->setFont(&TomThumb);
  dma_display->setTextSize(1);
  dma_display->setTextColor(borderColor);
  dma_display->setCursor(labelCursorX, labelCursorY);
  dma_display->print(label);

  const int keyLeft = boxX + (boxW - keyW) / 2;
  const int keyTop = labelTop + labelH + gap;
  const int keyCursorX = keyLeft - keyX1;
  const int keyCursorY = keyTop - keyY1;

  dma_display->setFont(&FreeSans9pt7b);
  dma_display->setTextSize(1);
  dma_display->setTextColor(borderColor);
  dma_display->setCursor(keyCursorX, keyCursorY);
  dma_display->print(passkeyStr);

}

uint16_t colorWheel(uint8_t pos)
{
  if (pos < 85)
  {
    return dma_display->color565(pos * 3, 255 - pos * 3, 0);
  }
  else if (pos < 170)
  {
    pos -= 85;
    return dma_display->color565(255 - pos * 3, 0, pos * 3);
  }
  pos -= 170;
  return dma_display->color565(0, pos * 3, 255 - pos * 3);
}

void initializeScrollingText()
{
  if (!dma_display)
  {
    return;
  }

  dma_display->setFont(&FreeSans9pt7b);
  dma_display->setTextSize(1);
  dma_display->setTextWrap(false);

  int16_t x1 = 0;
  int16_t y1 = 0;
  uint16_t w = 0;
  uint16_t h = 0;
  dma_display->getTextBounds(txt, 0, 0, &x1, &y1, &w, &h);

  textX = dma_display->width();
  textY = ((dma_display->height() - h) / 2) - y1;
  textMin = -static_cast<int16_t>(w + SCROLL_TEXT_GAP);

  auto &scrollState = scroll::state();
  scroll::resetTiming();
  const unsigned long now = millis();
  scrollState.lastScrollTickMs = now;
  scrollState.lastBackgroundTickMs = now;
  scrollState.backgroundOffset = 0;
  scrollState.colorOffset = 0;
  scrollState.textInitialized = true;
}

static void drawScrollingBackground(uint8_t offset)
{
  if (!dma_display)
  {
    return;
  }

  const int width = dma_display->width();
  const int height = dma_display->height();

  dma_display->clearScreen();
  for (int y = 0; y < height; ++y)
  {
    const uint8_t paletteIndex = sin8(static_cast<uint8_t>(y * 8) + offset);
    const CRGB color = ColorFromPalette(CloudColors_p, paletteIndex);
    dma_display->drawFastHLine(0, y, width, dma_display->color565(color.r, color.g, color.b));
  }
}

static void renderScrollingTextView()
{
  auto &scrollState = scroll::state();
  if (!scrollState.textInitialized)
  {
    initializeScrollingText();
  }

  const unsigned long now = millis();

  if (now - scrollState.lastBackgroundTickMs >= SCROLL_BACKGROUND_INTERVAL_MS)
  {
    scrollState.lastBackgroundTickMs = now;
    ++scrollState.backgroundOffset;
  }

  // drawScrollingBackground(scrollState.backgroundOffset);

  if (now - scrollState.lastScrollTickMs >= scrollState.textIntervalMs)
  {
    scrollState.lastScrollTickMs = now;
    --textX;
    if (textX <= textMin)
    {
      textX = dma_display->width() + SCROLL_TEXT_GAP;
    }
    scrollState.colorOffset = static_cast<uint8_t>(scrollState.colorOffset + 3);
  }

  drawText(scrollState.colorOffset);
}

uint16_t plasmaSpeed = 2; // Lower = slower animation

void drawPlasmaXbm(int x, int y, int width, int height, const uint8_t *xbm,
                   uint8_t time_offset = 0, float scale = 5.0f, float animSpeed = 0.2f,
                   uint8_t brightnessScale = 255)
{
  const int byteWidth = (width + 7) >> 3;
  if (byteWidth <= 0)
  {
    return;
  }

  const bool useStaticColorMode = staticColorModeEnabled;
  const uint16_t combinedBrightnessScale = static_cast<uint16_t>(
      (static_cast<uint32_t>(globalBrightnessScaleFixed) * static_cast<uint16_t>(brightnessScale) + 128u) >> 8);
  CRGB staticColorRgb = CRGB::Black;
  if (useStaticColorMode)
  {
    ensureStaticColorLoaded();
    staticColorRgb = gStaticColorState.color;
    staticColorRgb.r = static_cast<uint8_t>((static_cast<uint16_t>(staticColorRgb.r) * combinedBrightnessScale + 128) >> 8);
    staticColorRgb.g = static_cast<uint8_t>((static_cast<uint16_t>(staticColorRgb.g) * combinedBrightnessScale + 128) >> 8);
    staticColorRgb.b = static_cast<uint8_t>((static_cast<uint16_t>(staticColorRgb.b) * combinedBrightnessScale + 128) >> 8);
  }

  const uint16_t scaleFixed = static_cast<uint16_t>(scale * 256.0f);
  if (scaleFixed == 0)
  {
    return;
  }
  const uint16_t scaleHalfFixed = scaleFixed >> 1;
  int32_t startXFixed = static_cast<int32_t>(x) * static_cast<int32_t>(scaleFixed);

  const uint16_t animSpeedFixed = static_cast<uint16_t>(animSpeed * 256.0f);
  const uint32_t effectiveTimeFixed = static_cast<uint32_t>(time_counter) * animSpeedFixed;
  const uint8_t t = static_cast<uint8_t>(effectiveTimeFixed >> 8);
  const uint8_t t2 = static_cast<uint8_t>(t >> 1);
  const uint8_t t3 = static_cast<uint8_t>(((effectiveTimeFixed / 3U) >> 8) & 0xFF);

  const uint16_t brightnessScaleFixed = combinedBrightnessScale;
  const CRGB *paletteLut = useStaticColorMode ? nullptr : getScaledPlasmaPaletteLut(brightnessScaleFixed);

  for (int j = 0; j < height; ++j)
  {
    const int yj = y + j;
    const int32_t yScaleFixed = static_cast<int32_t>(yj) * scaleFixed;
    const uint8_t cos_val = cos8(static_cast<uint8_t>((yScaleFixed >> 8) + t2));
    int32_t tempFixed = static_cast<int32_t>(x + yj) * scaleHalfFixed;
    int32_t xFixed = startXFixed;
    const uint8_t *rowPtr = reinterpret_cast<const uint8_t *>(xbm) + j * byteWidth;

    for (int byteIndex = 0; byteIndex < byteWidth; ++byteIndex)
    {
      const uint8_t rowBits = rowPtr[byteIndex];
      const int pixelBase = byteIndex << 3;
      int pixelsInByte = width - pixelBase;
      if (pixelsInByte > 8)
      {
        pixelsInByte = 8;
      }
      if (pixelsInByte <= 0)
      {
        break;
      }

      if (rowBits == 0)
      {
        xFixed += static_cast<int32_t>(scaleFixed) * pixelsInByte;
        tempFixed += static_cast<int32_t>(scaleHalfFixed) * pixelsInByte;
        continue;
      }

      for (int bitIndex = 0; bitIndex < pixelsInByte; ++bitIndex)
      {
        if (rowBits & static_cast<uint8_t>(0x80U >> bitIndex))
        {
          const int pixelX = x + pixelBase + bitIndex;
          if (useStaticColorMode)
          {
            drawPixelRgbFast(pixelX, yj, staticColorRgb.r, staticColorRgb.g, staticColorRgb.b);
          }
          else
          {
            const uint8_t sin_val = sin8(static_cast<uint8_t>((xFixed >> 8) + t));
            const uint8_t sin_val2 = sin8(static_cast<uint8_t>((tempFixed >> 8) + t3));
            const uint8_t v = sin_val + cos_val + sin_val2;

            const uint8_t paletteIndex = static_cast<uint8_t>(v + time_offset);
            const CRGB &color = paletteLut[paletteIndex];

            drawPixelRgbFast(pixelX, yj, color.r, color.g, color.b);
          }
        }

        xFixed += scaleFixed;
        tempFixed += scaleHalfFixed;
      }
    }
  }
}

void drawText(int colorWheelOffset)
{
  // Set text properties
  dma_display->setFont(&FreeSans9pt7b);
  dma_display->setTextSize(1);

  // Use the color wheel for text color
  uint16_t textColor = colorWheel(colorWheelOffset);
  dma_display->setTextColor(textColor);

  // Set cursor and print text
  dma_display->setCursor(textX, textY);
  dma_display->print(txt);
}

// NEW function to draw bitmap with blink squash effect
void drawBitmapWithBlink(int x, int y, int width, int height, const uint8_t *bitmap, uint16_t color, int progress)
{
  int byteWidth = (width + 7) / 8;
  float center_y = (height - 1) / 2.0f;

  // This formula replicates the "w" calculation from the emulator for the squash effect
  float w = 0.005f + (1.0f - 0.005f) * (progress / 100.0f);

  for (int j = 0; j < height; j++)
  {
    for (int i = 0; i < width; i++)
    {
      if (pgm_read_byte(&bitmap[j * byteWidth + i / 8]) & (0x80 >> (i % 8)))
      {
        // Calculate brightness based on vertical distance from center, modulated by 'w'
        float blinkBrightness = pow(2, -w * pow(j - center_y, 2));
        if (blinkBrightness < 0.01)
          continue;

        // Apply the main color modulated by the blink brightness
        uint8_t r = (color >> 11) & 0x1F;
        uint8_t g = (color >> 5) & 0x3F;
        uint8_t b = color & 0x1F;

        r = r * blinkBrightness;
        g = g * blinkBrightness;
        b = b * blinkBrightness;

        dma_display->drawPixel(x + i, y + j, dma_display->color565(r << 3, g << 2, b << 3));
      }
    }
  }
}

// NEW ADVANCED function to draw a bitmap with optional plasma and blink squash effects
void drawBitmapAdvanced(int x, int y, int width, int height, const uint8_t *bitmap,
                        uint16_t color, int progress, bool usePlasma,
                        uint8_t time_offset = 0, float scale = 5.0, float animSpeed = 0.2f)
{
  const int byteWidth = (width + 7) / 8;
  const float center_y = (height - 1) / 2.0f;

  // --- Blink Effect Calculation ---
  const float w = 0.005f + (1.0f - 0.005f) * (progress / 100.0f);

  const bool enablePlasma = usePlasma && !staticColorModeEnabled;
  CRGB staticColorValue;
  if (staticColorModeEnabled)
  {
    ensureStaticColorLoaded();
    staticColorValue = gStaticColorState.color;
  }
  else
  {
    const uint8_t r = static_cast<uint8_t>((((color >> 11) & 0x1F) * 255) / 31);
    const uint8_t g = static_cast<uint8_t>((((color >> 5) & 0x3F) * 255) / 63);
    const uint8_t b = static_cast<uint8_t>(((color & 0x1F) * 255) / 31);
    staticColorValue = CRGB(r, g, b);
  }

  // --- Plasma Effect Setup (if enabled) ---
  uint8_t t = 0, t2 = 0, t3 = 0;
  float scaleHalf = 0;
  if (enablePlasma)
  {
    float effectiveTimeFloat = time_counter * animSpeed;
    t = (uint8_t)effectiveTimeFloat;
    t2 = (uint8_t)(effectiveTimeFloat / 2);
    t3 = (uint8_t)(effectiveTimeFloat / 3);
    scaleHalf = scale * 0.5f;
  }

  for (int j = 0; j < height; j++)
  {
    // --- OPTIMIZATION: Calculate blink brightness once per row ---
    const float blinkBrightness = powf(2.0f, -w * powf(j - center_y, 2));
    // If the entire row is too dim to be visible, skip it completely.
    if (blinkBrightness < 0.01f)
    {
      continue;
    }

    const uint16_t rowBrightnessScale = static_cast<uint16_t>(blinkBrightness * globalBrightnessScaleFixed);
    CRGB flatRowColor = CRGB::Black;
    if (!enablePlasma)
    {
      flatRowColor = staticColorValue;
      flatRowColor.r = static_cast<uint8_t>((static_cast<uint16_t>(flatRowColor.r) * rowBrightnessScale + 128) >> 8);
      flatRowColor.g = static_cast<uint8_t>((static_cast<uint16_t>(flatRowColor.g) * rowBrightnessScale + 128) >> 8);
      flatRowColor.b = static_cast<uint8_t>((static_cast<uint16_t>(flatRowColor.b) * rowBrightnessScale + 128) >> 8);
    }

    // Pre-calculate plasma values that are constant for the row
    const float y_val_plasma = (y + j) * scale;
    float temp_val_plasma = (x + y + j) * scaleHalf;
    float x_val_plasma = x * scale;
    uint8_t cos_val_plasma = 0;
    if (enablePlasma)
    {
      cos_val_plasma = cos8(y_val_plasma + t2);
    }

    const uint8_t *rowPtr = bitmap + (j * byteWidth);
    for (int byteIndex = 0; byteIndex < byteWidth; ++byteIndex)
    {
      const uint8_t rowBits = pgm_read_byte(rowPtr + byteIndex);
      const int pixelBase = byteIndex << 3;
      int pixelsInByte = width - pixelBase;
      if (pixelsInByte > 8)
      {
        pixelsInByte = 8;
      }
      if (pixelsInByte <= 0)
      {
        break;
      }

      if (rowBits == 0)
      {
        if (enablePlasma)
        {
          x_val_plasma += scale * pixelsInByte;
          temp_val_plasma += scaleHalf * pixelsInByte;
        }
        continue;
      }

      for (int bitIndex = 0; bitIndex < pixelsInByte; ++bitIndex)
      {
        if (rowBits & static_cast<uint8_t>(0x80U >> bitIndex))
        {
          const int pixelX = x + pixelBase + bitIndex;
        if (enablePlasma)
        {
          // --- Plasma Color Calculation ---
          uint8_t sin_val = sin8(x_val_plasma + t);
          uint8_t sin_val2 = sin8(temp_val_plasma + t3);
          uint8_t v = sin_val + cos_val_plasma + sin_val2;
            CRGB final_color = ColorFromPalette(currentPalette, v + time_offset);
            final_color.r = static_cast<uint8_t>((static_cast<uint16_t>(final_color.r) * rowBrightnessScale + 128) >> 8);
            final_color.g = static_cast<uint8_t>((static_cast<uint16_t>(final_color.g) * rowBrightnessScale + 128) >> 8);
            final_color.b = static_cast<uint8_t>((static_cast<uint16_t>(final_color.b) * rowBrightnessScale + 128) >> 8);
            drawPixelRgbFast(pixelX, y + j, final_color.r, final_color.g, final_color.b);
          }
          else
          {
            drawPixelRgbFast(pixelX, y + j, flatRowColor.r, flatRowColor.g, flatRowColor.b);
          }
        }

        if (enablePlasma)
        {
          x_val_plasma += scale;
          temp_val_plasma += scaleHalf;
        }
      }
    }
  }
}

// NEW: Update idle eye hover animation
void updateIdleHoverAnimation()
{
  const uint32_t currentMs = millis();

  const int prevIdleYOffset = idleEyeYOffset;
  const int prevIdleXOffset = idleEyeXOffset;

  if (IDLE_HOVER_PERIOD_MS_Y > 0)
  {
    const uint32_t phaseY = (static_cast<uint64_t>(currentMs % IDLE_HOVER_PERIOD_MS_Y) << 16) / IDLE_HOVER_PERIOD_MS_Y;
    const int32_t sinY = sin16(static_cast<uint16_t>(phaseY));
    idleEyeYOffset = static_cast<int>((sinY * IDLE_HOVER_AMPLITUDE_Y + 16384) >> 15);
  }
  else
  {
    idleEyeYOffset = 0;
  }

  if (IDLE_HOVER_PERIOD_MS_X > 0)
  {
    const uint32_t phaseX = (static_cast<uint64_t>(currentMs % IDLE_HOVER_PERIOD_MS_X) << 16) / IDLE_HOVER_PERIOD_MS_X;
    const int32_t cosX = cos16(static_cast<uint16_t>(phaseX));
    idleEyeXOffset = static_cast<int>((cosX * IDLE_HOVER_AMPLITUDE_X + 16384) >> 15);
  }
  else
  {
    idleEyeXOffset = 0;
  }

  if (idleEyeYOffset != prevIdleYOffset || idleEyeXOffset != prevIdleXOffset)
  {
    facePlasmaDirty = true;
  }
}

// NEW: Update eye bounce animation (Modified for multiple bounces)
void updateEyeBounceAnimation()
{
  int newOffset = currentEyeYOffset;

  if (!isEyeBouncing)
  {
    eyeBounceCount = 0;
    if (newOffset != 0)
    {
      newOffset = 0;
      facePlasmaDirty = true;
    }
    currentEyeYOffset = newOffset;
    return;
  }

  unsigned long elapsed = millis() - eyeBounceStartTime;

  if (elapsed >= EYE_BOUNCE_DURATION)
  {
    eyeBounceCount++;
    if (eyeBounceCount >= MAX_EYE_BOUNCES)
    {
      isEyeBouncing = false;
      newOffset = 0;
      eyeBounceCount = 0;

      if (currentView == VIEW_CIRCLE_EYES)
      {
        currentView = viewBeforeEyeBounce;
        scheduleLastViewPersist(currentView);
        requestDisplayRefresh();
        LOG_DEBUG("Eye bounce finished. Reverting to view: %d\n", currentView);
        notifyBleTask();
      }

      if (currentEyeYOffset != newOffset)
      {
        facePlasmaDirty = true;
      }
      currentEyeYOffset = newOffset;
      return;
    }
    else
    {
      eyeBounceStartTime = millis();
      elapsed = 0;
    }
  }

  const unsigned long halfDuration = EYE_BOUNCE_DURATION / 2;
  if (elapsed < halfDuration)
  {
    newOffset = map(elapsed, 0, halfDuration, 0, EYE_BOUNCE_AMPLITUDE);
  }
  else
  {
    newOffset = map(elapsed, halfDuration, EYE_BOUNCE_DURATION, EYE_BOUNCE_AMPLITUDE, 0);
  }

  if (newOffset != currentEyeYOffset)
  {
    currentEyeYOffset = newOffset;
    facePlasmaDirty = true;
  }
}

void drawPlasmaFace()
{
  // Keep idle hover offsets in sync even if the main loop stalls
  updateIdleHoverAnimation();

  // Combine bounce and idle hover offsets
  // Combine bounce and idle hover offsets
  int final_y_offset = currentEyeYOffset + idleEyeYOffset;
  int final_x_offset_right = idleEyeXOffset; // X offset for the right eye (0,0 based)
  int final_x_offset_left = idleEyeXOffset;  // X offset for the left eye (96,0 based)

  // Draw eyes with different plasma parameters
  int y_offset = currentEyeYOffset;                                                    // Get current bounce offset for eyes
  drawPlasmaXbm(0 + final_x_offset_right, 0 + final_y_offset, 32, 16, Eye, 0, 1.0);    // Right eye
  drawPlasmaXbm(96 + final_x_offset_left, 0 + final_y_offset, 32, 16, EyeL, 128, 1.0); // Left eye (phase offset)
  // Nose with finer pattern
  drawPlasmaXbm(56, 14, 8, 8, nose, 64, 2.0);
  drawPlasmaXbm(64, 14, 8, 8, noseL, 64, 2.0);
  // Mouth with larger pattern

  // drawPlasmaXbm(0, 16, 64, 16, maw, 32, 2.0);
  // drawPlasmaXbm(64, 16, 64, 16, mawL, 32, 2.0);

  const uint8_t mouthBrightness = micGetMouthBrightness();
  drawPlasmaXbm(0, 10, 64, 22, maw2Closed, 32, 0.5f, 0.2f, mouthBrightness);
  drawPlasmaXbm(64, 10, 64, 22, maw2ClosedL, 32, 0.5f, 0.2f, mouthBrightness);
}

void updatePlasmaFace()
{
  static unsigned long lastUpdate = 0;
  static unsigned long lastPaletteChange = 0;
  const uint16_t frameDelay = 2;               // Delay for plasma animation update
  const unsigned long paletteInterval = 10000; // Change palette every 10 seconds
  unsigned long now = millis();

  if (now - lastUpdate >= frameDelay)
  {
    lastUpdate = now;
    time_counter += plasmaSpeed; // Update plasma animation counter
    facePlasmaDirty = true;
  }

  if (now - lastPaletteChange > paletteInterval)
  {
    lastPaletteChange = now;
    currentPalette = palettes[random(0, sizeof(palettes) / sizeof(palettes[0]))];
    facePlasmaDirty = true;
  }
}

// SETUP - RUNS ONCE AT PROGRAM START --------------------------------------

void displayLoadingBar()
{
  // dma_display->clearScreen();

  // Draw the loading bar background
  int barWidth = dma_display->width() - 80;               // Width of the loading bar
  int barHeight = 5;                                      // Height of the loading bar
  int barX = (dma_display->width() / 4) - (barWidth / 2); // Center X position
  int barY = (dma_display->height() - barHeight) / 2;     // Center vertically
  // int barRadius = 4;                // Rounded corners
  dma_display->drawRect(barX, (barY * 2), barWidth, barHeight, dma_display->color565(9, 9, 9));
  dma_display->drawRect(barX + 64, (barY * 2), barWidth, barHeight, dma_display->color565(9, 9, 9));

  // Draw the loading progress
  int progressWidth = (barWidth - 2) * loadingProgress / loadingMax;
  dma_display->fillRect(barX + 1, (barY * 2) + 1, progressWidth, barHeight - 2, dma_display->color565(255, 255, 255));
  dma_display->fillRect((barX + 1) + 64, (barY * 2) + 1, progressWidth, barHeight - 2, dma_display->color565(255, 255, 255));
}

void initBlinkState()
{
  blinkState.startTime = 0;
  blinkState.durationMs = 700;
  blinkState.closeDuration = 50;
  blinkState.holdDuration = 20;
  blinkState.openDuration = 50;
}

void updateBlinkAnimationOld()
{
  static bool initialized = false;
  if (!initialized)
  {
    initBlinkState();
    initialized = true;
  }
  const unsigned long now = millis();

  if (isBlinking)
  {
    const unsigned long elapsed = now - blinkState.startTime;
    const unsigned long closeDur = blinkState.closeDuration;
    const unsigned long holdDur = blinkState.holdDuration;
    const unsigned long openDur = blinkState.openDuration;
    const unsigned long totalDuration = closeDur + holdDur + openDur;

    // Safety and validity checks: terminate blink if too much time has elapsed or if phase durations are invalid.
    if (elapsed > blinkState.durationMs * 2 || totalDuration == 0)
    {
      isBlinking = false;
      setBlinkProgress(0);
      lastEyeBlinkTime = now;
      nextBlinkDelay = random(minBlinkDelay, maxBlinkDelay);
      return;
    }

    // Pre-calculate phase thresholds.
    const unsigned long phase1End = closeDur;           // End of closing phase.
    const unsigned long phase2End = closeDur + holdDur; // End of hold phase.

    if (elapsed < phase1End)
    {
      // Closing phase with integer ease-in.
      const unsigned long scaledProgress = (elapsed * 100UL) / closeDur;
      const unsigned long eased = (scaledProgress * scaledProgress + 50UL) / 100UL;
      setBlinkProgress(static_cast<int>(eased > 100UL ? 100UL : eased));
    }
    else if (elapsed < phase2End)
    {
      // Hold phase – full blink.
      setBlinkProgress(100);
    }
    else if (elapsed < totalDuration)
    {
      // Opening phase with integer ease-out.
      const unsigned long openElapsed = elapsed - phase2End;
      const unsigned long scaledProgress = (openElapsed * 100UL) / openDur;
      const unsigned long inv = (scaledProgress > 100UL) ? 0UL : (100UL - scaledProgress);
      const unsigned long eased = 100UL - ((inv * inv + 50UL) / 100UL);
      setBlinkProgress(static_cast<int>(eased > 100UL ? 100UL : eased));
    }
    else
    {
      // Blink cycle complete; reset variables.
      isBlinking = false;
      setBlinkProgress(0);
      lastEyeBlinkTime = now;
      nextBlinkDelay = random(minBlinkDelay, maxBlinkDelay);
    }
  }
  else if (now - lastEyeBlinkTime >= nextBlinkDelay)
  {
    isBlinking = true;
    blinkState.startTime = now;
    blinkState.durationMs = random(minBlinkDuration, maxBlinkDuration);
    blinkState.closeDuration = blinkState.durationMs * 0.30;
    blinkState.holdDuration = blinkState.durationMs * 0.15;
    blinkState.openDuration = blinkState.durationMs * 0.55;
    blinkProgress = 0;
  }
}

void updateBlinkAnimation()
{
  const unsigned long now = millis();

  // Check if it's time to start a new blink
  if (manualBlinkTrigger || (!isBlinking && (now - lastEyeBlinkTime >= nextBlinkDelay)))
  {
    isBlinking = true;
    manualBlinkTrigger = false;
    blinkState.startTime = now;

    // Randomize total blink duration and phase ratios, like in the emulator
    unsigned long totalDuration = 400 + random(301); // 400 to 700 ms
    float closeRatio = 0.2f + (random(21) / 100.0f); // 0.2 to 0.4
    float holdRatio = 0.05f + (random(11) / 100.0f); // 0.05 to 0.15

    blinkState.closeDuration = totalDuration * closeRatio;
    blinkState.holdDuration = totalDuration * holdRatio;
    blinkState.openDuration = totalDuration * (1.0f - closeRatio - holdRatio);
    blinkProgress = 0;
  }

  // If a blink is in progress, update its state
  if (isBlinking)
  {
    const unsigned long elapsed = now - blinkState.startTime;
    const unsigned long totalDuration = blinkState.closeDuration + blinkState.holdDuration + blinkState.openDuration;

    if (elapsed < blinkState.closeDuration)
    {
      float t = (float)elapsed / blinkState.closeDuration;
      blinkProgress = 100 * easeInQuad(t);
    }
    else if (elapsed < blinkState.closeDuration + blinkState.holdDuration)
    {
      blinkProgress = 100;
    }
    else if (elapsed < totalDuration)
    {
      float t = (float)(elapsed - blinkState.closeDuration - blinkState.holdDuration) / blinkState.openDuration;
      blinkProgress = 100 * (1.0f - easeOutQuad(t));
    }
    else
    {
      // Blink finished
      isBlinking = false;
      blinkProgress = 0;
      lastEyeBlinkTime = now;
      // Calculate delay for the next blink
      nextBlinkDelay = minBlinkDelay + random(maxBlinkDelay - minBlinkDelay + 1);
      // 15% chance of a quick "double blink"
      if (random(100) < 15)
      {
        nextBlinkDelay = 100 + random(151);
      }
    }
  }
}

// Optimized rotation function
void drawRotatedBitmap(int16_t x, int16_t y, const uint8_t *bitmap,
                       uint16_t w, uint16_t h, float cosA, float sinA,
                       uint16_t color)
{
  const int16_t cx = w / 2;
  const int16_t cy = h / 2;
  const int byteWidth = (w + 7) >> 3;

  const int32_t cosFixed = static_cast<int32_t>(cosA * 65536.0f);
  const int32_t sinFixed = static_cast<int32_t>(sinA * 65536.0f);

  for (uint16_t j = 0; j < h; ++j)
  {
    const int16_t dy = static_cast<int16_t>(j) - cy;
    int32_t baseXFixed = (static_cast<int32_t>(-cx) * cosFixed) - (static_cast<int32_t>(dy) * sinFixed);
    int32_t baseYFixed = (static_cast<int32_t>(-cx) * sinFixed) + (static_cast<int32_t>(dy) * cosFixed);

    baseXFixed += static_cast<int32_t>(x) << 16;
    baseYFixed += static_cast<int32_t>(y) << 16;

    int32_t pixelXFixed = baseXFixed;
    int32_t pixelYFixed = baseYFixed;
    uint8_t currentByte = 0;

    for (uint16_t i = 0; i < w; ++i)
    {
      if ((i & 7U) == 0U)
      {
        currentByte = pgm_read_byte(&bitmap[j * byteWidth + (i >> 3)]);
      }

      if (currentByte & (0x80U >> (i & 7U)))
      {
        const int16_t newX = static_cast<int16_t>(pixelXFixed >> 16);
        const int16_t newY = static_cast<int16_t>(pixelYFixed >> 16);

        if (newX >= 0 && newX < PANE_WIDTH && newY >= 0 && newY < PANE_HEIGHT)
        {
          dma_display->drawPixel(newX, newY, color);
        }
      }

      pixelXFixed += cosFixed;
      pixelYFixed += sinFixed;
    }
  }
}

void updateRotatingSpiral()
{
  static unsigned long lastUpdate = 0;
  static float currentAngle = 0;
  const unsigned long rotationFrameInterval = 11; // ~90 FPS (11ms per frame)
  unsigned long currentTime = millis();

  if (currentTime - lastUpdate < rotationFrameInterval)
    return;
  lastUpdate = currentTime;

  // Calculate color once per frame
  static uint8_t colorIndex = 0;
  colorIndex += 2; // Slower color transition
  CRGB currentColor = ColorFromPalette(currentPalette, colorIndex);
  uint16_t color = dma_display->color565(currentColor.r, currentColor.g, currentColor.b);

  // Optimized rotation with pre-calculated values
  float radians = currentAngle * PI / 180.0;
  float cosA = cos(radians);
  float sinA = sin(radians);
  currentAngle = fmod(currentAngle + 4, 360); // Slower rotation (4° per frame)

  // Clear previous spiral frame
  // dma_display->fillRect(24, 15, 32, 25, 0);
  // dma_display->fillRect(105, 15, 32, 25, 0);
  //  Draw both spirals using pre-transformed coordinates
  drawRotatedBitmap(24, 15, spiral, 25, 25, cosA, sinA, color);
  drawRotatedBitmap(105, 15, spiralL, 25, 25, cosA, sinA, color);
}

// Draw the blinking eyes
void blinkingEyes()
{
  // Combine bounce and idle hover offsets
  int final_y_offset = currentEyeYOffset + idleEyeYOffset;
  int final_x_offset = idleEyeXOffset;

  // --- Eye Configuration ---
  const uint8_t *rightEyeBitmap = (const uint8_t *)slanteyes; // Default
  const uint8_t *leftEyeBitmap = (const uint8_t *)slanteyesL; // Default
  int eyeWidth = 24, eyeHeight = 16;
  int rightEyeX = 2, rightEyeY = 0;
  int leftEyeX = 102, leftEyeY = 0;
  bool usePlasma = true;                                    // Most views use plasma
  uint16_t solidColor = dma_display->color565(0, 255, 255); // Default cyan, not used if usePlasma is true

  // Select eye assets and properties based on the current view
  switch (currentView)
  {
  case VIEW_NORMAL_FACE: // Normal
  case VIEW_BLUSH_FACE:  // Blush
    rightEyeBitmap = (const uint8_t *)Eye;
    leftEyeBitmap = (const uint8_t *)EyeL;
    eyeWidth = 32;
    eyeHeight = 16;
    rightEyeX = 0;
    rightEyeY = 0;
    leftEyeX = 96;
    leftEyeY = 0;
    break;
  case VIEW_SEMICIRCLE_EYES: // Semicircle
    rightEyeBitmap = (const uint8_t *)semicircleeyes;
    leftEyeBitmap = (const uint8_t *)semicircleeyes; // Same bitmap, different plasma offset
    eyeWidth = 32;
    eyeHeight = 12;
    rightEyeX = 2;
    rightEyeY = 2;
    leftEyeX = 94;
    leftEyeY = 2;
    break;
  case VIEW_X_EYES: // X eyes
    rightEyeBitmap = (const uint8_t *)x_eyes;
    leftEyeBitmap = (const uint8_t *)x_eyes; // Same bitmap
    eyeWidth = 31;
    eyeHeight = 15;
    rightEyeX = 0;
    rightEyeY = 0;
    leftEyeX = 96;
    leftEyeY = 0;
    break;
  case VIEW_SLANT_EYES: // Slant eyes (This is also the default)
    // Values are already set by default
    break;
  case VIEW_SPIRAL_EYES: // Spiral eyes
    return;              // Spiral view handles its own drawing, so we exit here.
  case VIEW_CIRCLE_EYES: // Circle eyes
    rightEyeBitmap = (const uint8_t *)circleeyes;
    leftEyeBitmap = (const uint8_t *)circleeyes; // Same bitmap
    eyeWidth = 25;
    eyeHeight = 21;
    rightEyeX = 10;
    rightEyeY = 2;
    leftEyeX = 93;
    leftEyeY = 2;
    break;
  case VIEW_ALT_FACE:
  {
    const uint16_t pupilColor = dma_display->color565(0, 0, 0);
    constexpr uint8_t kAltEyeTimeOffsetRight = 80;
    constexpr uint8_t kAltEyeTimeOffsetLeft = 208;

    drawBitmapAdvanced(10 + final_x_offset, 4 + final_y_offset, 23, 13, Alt_scleraR,
                       solidColor, blinkProgress, true, kAltEyeTimeOffsetRight);
    drawBitmapAdvanced(95 + final_x_offset, 4 + final_y_offset, 23, 13, Alt_scleraL,
                       solidColor, blinkProgress, true, kAltEyeTimeOffsetLeft);
    drawBitmapWithBlink(19 + final_x_offset, 5 + final_y_offset, 3, 11, AltFacePupil,
                        pupilColor, blinkProgress);
    drawBitmapWithBlink(106 + final_x_offset, 5 + final_y_offset, 3, 11, AltFacePupil,
                        pupilColor, blinkProgress);
    return;
  }
    // Default case uses the slanteyes defined before the switch
  }

  // Draw the right eye (viewer's perspective)
  drawBitmapAdvanced(rightEyeX + final_x_offset, rightEyeY + final_y_offset,
                     eyeWidth, eyeHeight, rightEyeBitmap, solidColor,
                     blinkProgress, usePlasma, 0);

  // Draw the left eye with a plasma phase offset if plasma is used
  drawBitmapAdvanced(leftEyeX + final_x_offset, leftEyeY + final_y_offset,
                     eyeWidth, eyeHeight, leftEyeBitmap, solidColor,
                     blinkProgress, usePlasma, 128);
}

// Function to disable/clear the blush display when the effect is over
void disableBlush()
{
  Serial.println("Blush disabled!");
  facePlasmaDirty = true; // Defer actual redraw to display task
}

// Update the blush effect state (non‑blocking)
void updateBlush()
{
  unsigned long now = millis();
  unsigned long elapsed = now - blushStateStartTime;

  switch (blushState)
  {
  case BlushState::FadeIn:
    if (elapsed < fadeInDuration)
    {
      // Use ease-in for a smooth start
      float progress = (float)elapsed / fadeInDuration;
      blushBrightness = 255 * easeInQuad(progress);
    }
    else
    {
      blushBrightness = 255;
      blushState = BlushState::Full;
      blushStateStartTime = now; // Restart timer for full brightness phase
    }
    break;

  case BlushState::Full:
    if (elapsed >= fullDuration)
    {
      // After full brightness time, start fading out
      blushState = BlushState::FadeOut;
      blushStateStartTime = now; // Restart timer for fade-out
    }
    // Brightness remains at 255 during this phase.
    break;

  case BlushState::FadeOut:
    if (elapsed < fadeOutDuration)
    {
      // Use ease-out for a smooth end
      float progress = (float)elapsed / fadeOutDuration;
      blushBrightness = 255 * (1.0f - easeOutQuad(progress));
    }
    else
    {
      blushBrightness = 0;
      blushState = BlushState::Inactive;
      disableBlush();

      // Revert to previous view if this was an overlay blush
      if (wasBlushOverlay)
      {
        currentView = originalViewBeforeBlush;
        scheduleLastViewPersist(currentView);
        requestDisplayRefresh();
        Serial.printf("Blush overlay finished. Reverting to view: %d\n", currentView);
        notifyBleTask();
        wasBlushOverlay = false; // Reset the flag
      }
    }
    break;

  default:
    wasBlushOverlay = false; // Ensure flag is reset if state becomes inactive
    break;
  }
}

void drawBlush()
{
// MARK: Debug: Print blush brightness
#if DEBUG_VIEWS
  Serial.print("Blush brightness: ");
#endif
  // Set blush color based on brightness
  uint16_t blushColor = dma_display->color565(blushBrightness, 0, blushBrightness);

  const int blushWidth = 11;
  const int blushHeight = 13;
  const int blushCount = 3;
  const int blushSpacing = blushWidth - 4;
  const int blushY = 1;
  const int rightStartX = 33;
  const int leftStartX = 70;
  const int totalBlushWidth = blushWidth + (blushCount - 1) * blushSpacing;

  // MARK: Clear only the blush area to prevent artifacts
  // dma_display->fillRect(rightStartX, blushY, totalBlushWidth, blushHeight, 0);
  // dma_display->fillRect(leftStartX, blushY, totalBlushWidth, blushHeight, 0);

  for (int i = 0; i < blushCount; ++i)
  {
    drawXbm565(rightStartX + (i * blushSpacing), blushY, blushWidth, blushHeight, blush, blushColor);
    drawXbm565(leftStartX + (i * blushSpacing), blushY, blushWidth, blushHeight, blushL, blushColor);
  }
}

void drawTransFlag()
{
  // dma_display->clearScreen(); // Clear the display

  int stripeHeight = dma_display->height() / 5; // Height of each stripe

  // Define colors in RGB565 format with intended values:
  // lightBlue: (0,51,102), pink: (255,10,73), white: (255,255,255)
  uint16_t lightBlue = dma_display->color565(0, (102 / 2), (204 / 2));
  uint16_t pink = dma_display->color565(255, (20 / 2), (147 / 2));
  uint16_t white = dma_display->color565(255, 255, 255);

  // Draw stripes
  dma_display->fillRect(0, 0, dma_display->width(), stripeHeight, lightBlue);                // Top light blue stripe
  dma_display->fillRect(0, stripeHeight, dma_display->width(), stripeHeight, pink);          // Pink stripe
  dma_display->fillRect(0, stripeHeight * 2, dma_display->width(), stripeHeight, white);     // Middle white stripe
  dma_display->fillRect(0, stripeHeight * 3, dma_display->width(), stripeHeight, pink);      // Pink stripe
  dma_display->fillRect(0, stripeHeight * 4, dma_display->width(), stripeHeight, lightBlue); // Bottom light blue stripe
}

void drawLGBTFlag()
{
  int stripeHeight = dma_display->height() / 6; // Height of each stripe

  // Define colors in RGB565 format with intended values:
  // red: (255,0,0), orange: (255,127,0), yellow: (255,255,0), green: (0,255,0), blue: (0,0,255), purple: (139,0,255)
  uint16_t red = dma_display->color565(255, 0, 0);
  uint16_t orange = dma_display->color565(255, 127, 0);
  uint16_t yellow = dma_display->color565(255, 255, 0);
  uint16_t green = dma_display->color565(0, 255, 0);
  uint16_t blue = dma_display->color565(0, 0, 255);
  uint16_t purple = dma_display->color565(139, 0, 255);

  // Draw stripes
  dma_display->fillRect(0, 0, dma_display->width(), stripeHeight, red);                   // Top red stripe
  dma_display->fillRect(0, stripeHeight, dma_display->width(), stripeHeight, orange);     // Orange stripe
  dma_display->fillRect(0, stripeHeight * 2, dma_display->width(), stripeHeight, yellow); // Yellow stripe
  dma_display->fillRect(0, stripeHeight * 3, dma_display->width(), stripeHeight, green);  // Green stripe
  dma_display->fillRect(0, stripeHeight * 4, dma_display->width(), stripeHeight, blue);   // Blue stripe
  dma_display->fillRect(0, stripeHeight * 5, dma_display->width(), stripeHeight, purple); // Bottom purple stripe
}

void baseFace()
{
  // Update idle hover here so the render task always sees fresh offsets
  updateIdleHoverAnimation();

  // Combine all offsets for final positioning.
  // These will be used for any facial feature that should move with the hover/bounce effect.
  int final_y_offset = currentEyeYOffset + idleEyeYOffset;
  int final_x_offset = idleEyeXOffset;

  if (currentView == VIEW_ALT_FACE)
  {
    const uint16_t blushColor = dma_display->color565(255, 110, 150);
    drawPlasmaXbm(22 + final_x_offset, 0 + final_y_offset, 9, 7, Alt_BrowR, 16, 2.0f);
    drawPlasmaXbm(96 + final_x_offset, 0 + final_y_offset, 9, 7, Alt_BrowL, 144, 2.0f);
    drawXbm565(8 + final_x_offset, 16 + final_y_offset, 9, 5, Alt_BlushR, blushColor);
    drawXbm565(110 + final_x_offset, 16 + final_y_offset, 9, 5, Alt_BlushL, blushColor);
  }

  blinkingEyes(); // This function now correctly uses the global offsets internally

  const uint8_t mouthBrightness = micGetMouthBrightness();
  if (mouthOpen)
  {
    drawPlasmaXbm(0, 10, 64, 22, maw2, 0, 1.0f, 0.2f, mouthBrightness);
    drawPlasmaXbm(64, 10, 64, 22, maw2L, 128, 1.0f, 0.2f, mouthBrightness); // Left eye open (phase offset)
  }
  else
  {
    drawPlasmaXbm(0, 10, 64, 22, maw2Closed, 0, 1.0f, 0.2f, mouthBrightness);     // Right eye
    drawPlasmaXbm(64, 10, 64, 22, maw2ClosedL, 128, 1.0f, 0.2f, mouthBrightness); // Left eye (phase offset)
  }

  if (currentView == VIEW_ALT_FACE)
  {
    drawPlasmaXbm(58 + final_x_offset, 7 + final_y_offset, 4, 2, Alt_Nose, 96, 2.0f);
    drawPlasmaXbm(68 + final_x_offset, 7 + final_y_offset, 4, 2, Alt_Nose, 96, 2.0f);
  }
  else
  {
    drawPlasmaXbm(56, 4 + final_y_offset, 8, 8, nose, 64, 2.0);
    drawPlasmaXbm(64, 4 + final_y_offset, 8, 8, noseL, 64, 2.0);
  }

  facePlasmaDirty = false;
}

void staticColor()
{
  ensureStaticColorLoaded();
  const CRGB color = gStaticColorState.color;
  const uint16_t encodedColor = dma_display->color565(color.r, color.g, color.b);
  dma_display->fillScreen(encodedColor);
}

void patternPlasma()
{
  // Advance plasma animation timing + palette cycling reused by all plasma-based views
  updatePlasmaFace();

  static uint16_t paletteLUT[256];
  static CRGBPalette16 cachedPalette;
  static uint16_t cachedBrightnessScale = 0;
  static bool paletteLUTValid = false;

  if (!paletteLUTValid || cachedBrightnessScale != globalBrightnessScaleFixed || std::memcmp(&cachedPalette, &currentPalette, sizeof(CRGBPalette16)) != 0)
  {
    const uint16_t scale = globalBrightnessScaleFixed;
    for (int i = 0; i < 256; ++i)
    {
      CRGB color = ColorFromPalette(currentPalette, static_cast<uint8_t>(i));
      color.r = static_cast<uint8_t>((static_cast<uint16_t>(color.r) * scale + 128) >> 8);
      color.g = static_cast<uint8_t>((static_cast<uint16_t>(color.g) * scale + 128) >> 8);
      color.b = static_cast<uint8_t>((static_cast<uint16_t>(color.b) * scale + 128) >> 8);
      paletteLUT[i] = dma_display->color565(color.r, color.g, color.b);
    }
    cachedPalette = currentPalette;
    cachedBrightnessScale = scale;
    paletteLUTValid = true;
  }

  // Pre-calculate values that only depend on time_counter
  // These are constant for the entire frame draw
  const uint8_t wibble = sin8(time_counter);
  // Pre-calculate the cosine term and the division by 8 (as shift)
  // Note: cos8 argument is uint8_t, so -time_counter wraps around correctly.
  const uint8_t cos8_val_shifted = cos8(static_cast<uint8_t>(-time_counter)) >> 3; // Calculate cos8(-t)/8 once
  const uint16_t time_val = time_counter;                                          // Use a local copy for calculations
  const uint16_t term2_factor = 128 - wibble;                                      // Calculate 128 - sin8(t) once

  // Get display dimensions once
  const int display_width = dma_display->width();
  const int display_height = dma_display->height();
  int16_t columnWave[PANE_WIDTH];
  int16_t rowWave[PANE_HEIGHT];

  for (int x = 0; x < display_width; ++x)
  {
    columnWave[x] = sin16(x * wibble * 3 + time_val);
  }

  for (int y = 0; y < display_height; ++y)
  {
    rowWave[y] = cos16(y * term2_factor + time_val);
  }

  // Outer loop for X
  for (int x = 0; x < display_width; x++)
  {
    // Pre-calculate terms dependent only on X and time
    const int16_t term1_wave = columnWave[x];
    // The y*x part needs to be inside the Y loop, but we can precalculate x * cos8_val_shifted
    const uint16_t term3_x_factor = x * cos8_val_shifted;
    uint16_t term3Arg = 0;

    // Inner loop for Y
    for (int y = 0; y < display_height; y++)
    {
      // Calculate plasma value 'v'
      // Start with base offset
      int16_t v = 128;
      // Add terms using pre-calculated values where possible
      v += term1_wave;                         // sin16(x*wibble*3 + t)
      v += rowWave[y];                         // cos16(y*(128-wibble) + t)
      v += sin16(term3Arg);                    // sin16(y * (x * cos8(-t) >> 3))

      const uint16_t color565 = paletteLUT[static_cast<uint8_t>(v >> 8)];
      dma_display->drawPixel(x, y, color565);
      term3Arg = static_cast<uint16_t>(term3Arg + term3_x_factor);
    }
  }
}

void displaySleepMode()
{
  static unsigned long lastBlinkTime = 0;
  static bool eyesOpen = false;
  static float animationPhase = 0;
  static unsigned long lastAnimTime = 0;

  // Apply brightness adjustment for breathing effect
  static unsigned long lastBreathTime = 0;
  static float brightness = 0;
  static float breathingDirection = 1; // 1 for increasing, -1 for decreasing

  // Update breathing effect
  if (millis() - lastBreathTime >= 50)
  {
    lastBreathTime = millis();

    // Update breathing brightness
    brightness += breathingDirection * 0.01; // Slow breathing effect

    // Reverse direction at limits
    if (brightness >= 1.0)
    {
      brightness = 1.0;
      breathingDirection = -1;
    }
    else if (brightness <= 0.1)
    { // Keep a minimum brightness
      brightness = 0.1;
      breathingDirection = 1;
    }

    // Apply breathing effect to overall brightness
    uint8_t currentBrightness = sleepBrightness * brightness;
    dma_display->setBrightness8(currentBrightness);
    updateGlobalBrightnessScale(currentBrightness);
  }

  dma_display->clearScreen();

  // Simple animation for floating Zs
  if (millis() - lastAnimTime > 100)
  {
    animationPhase += 0.1;
    lastAnimTime = millis();
  }

  // Draw animated ZZZs with offset based on animation phase
  int offset1 = sin8(animationPhase * 20) / 16;
  int offset2 = sin8((animationPhase + 0.3) * 20) / 16;
  int offset3 = sin8((animationPhase + 0.6) * 20) / 16;

  drawXbm565(22, 2 + offset1, 8, 8, sleepZ1, dma_display->color565(150, 150, 150));
  drawXbm565(32, 0 + offset2, 8, 8, sleepZ2, dma_display->color565(100, 100, 100));
  drawXbm565(42, 1 + offset3, 8, 8, sleepZ3, dma_display->color565(50, 50, 50));

  drawXbm565(76, 2 + offset1, 8, 8, sleepZ1, dma_display->color565(50, 50, 50));
  drawXbm565(86, 0 + offset2, 8, 8, sleepZ2, dma_display->color565(100, 100, 100));
  drawXbm565(96, 1 + offset3, 8, 8, sleepZ3, dma_display->color565(150, 150, 150));

  /*
  // Draw closed or slightly open eyes
  if (millis() - lastBlinkTime > 4000)
  { // Occasional slow blink
    eyesOpen = !eyesOpen;
    lastBlinkTime = millis();
  }
*/
  // Draw nose
  drawXbm565(56, 14, 8, 8, nose, dma_display->color565(100, 100, 100));
  drawXbm565(64, 14, 8, 8, noseL, dma_display->color565(100, 100, 100));

  if (eyesOpen)
  {
    // Draw slightly open eyes - just a small slit
    // dma_display->fillRect(5, 5, 20, 2, dma_display->color565(150, 150, 150));
    // dma_display->fillRect(103, 5, 20, 2, dma_display->color565(150, 150, 150));
  }
  else
  {
    // Draw closed eyes
    // dma_display->drawLine(5, 10, 40, 12, dma_display->color565(150, 150, 150));
    // dma_display->drawLine(83, 10, 40, 12, dma_display->color565(150, 150, 150));
    // dma_display->drawLine(88, 12, 123, 10, dma_display->color565(150, 150, 150));
  }
  // Draw sleeping mouth (slight curve)
  /*
  drawXbm565(0, 20, 10, 8, maw2Closed, dma_display->color565(120, 120, 120));
  drawXbm565(64, 20, 10, 8, maw2ClosedL, dma_display->color565(120, 120, 120));
  */
  // dma_display->drawFastHLine(PANE_WIDTH / 2 - 15, PANE_HEIGHT - 8, 30, dma_display->color565(120, 120, 120));
  drawBluetoothStatusIcon();
  const PairingSnapshot pairingSnapshot = getPairingSnapshot();
  if (pairingSnapshot.pairing && pairingSnapshot.passkeyValid)
  {
    drawPairingPasskeyOverlay(pairingSnapshot.passkey);
  }
  dma_display->flipDMABuffer();
}

void initStarfield()
{
  for (int i = 0; i < NUM_STARS; i++)
  {
    stars[i].x = random(0, dma_display->width());
    stars[i].y = random(0, dma_display->height());
    stars[i].speed = random(1, 4); // Stars move at different speeds
  }
}

void updateStarfield()
{
  // Update and draw each star
  for (int i = 0; i < NUM_STARS; i++)
  {
    dma_display->drawPixel(stars[i].x, stars[i].y, dma_display->color565(255, 255, 255));
    // Move star leftwards based on its speed
    stars[i].x -= stars[i].speed;
    // Reset star to right edge if it goes off the left side
    if (stars[i].x < 0)
    {
      stars[i].x = dma_display->width() - 1;
      stars[i].y = random(0, dma_display->height());
      stars[i].speed = random(1, 4);
    }
  }
  // dma_display->flipDMABuffer();
}

void initDVDLogoAnimation()
{
  logos[0] = {0, 0, 1, 1, dma_display->color565(255, 0, 255)};   // Left panel
  logos[1] = {64, 0, -1, 1, dma_display->color565(0, 255, 255)}; // Right panel
}

void bleNotifyTask(void *param)
{
  for (;;)
  {
    // block here until someone calls xTaskNotifyGive()
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    PERF_SCOPE(PerfBucket::Ble);
#if DEBUG_MODE
    const uint32_t bleStartMicros = micros();
#endif
    if (deviceConnected)
    {
      uint8_t bleViewValue = static_cast<uint8_t>(currentView);
      pFaceCharacteristic->setValue(&bleViewValue, 1);
      pFaceCharacteristic->notify();
      pConfigCharacteristic->notify();
    }
#if DEBUG_MODE
    const uint32_t bleElapsed = micros() - bleStartMicros;
    if (bleElapsed > SLOW_SECTION_THRESHOLD_US)
    {
      Serial.printf("BLE notify slow: %lu us\n", static_cast<unsigned long>(bleElapsed));
    }
#endif
  }
}

// Dedicated rendering task keeps DMA flips on a steady cadence.
static void displayTask(void *pvParameters)
{
  (void)pvParameters;
  TickType_t lastWake = xTaskGetTickCount();
  enum class ViewTransitionPhase : uint8_t
  {
    Idle,
    FadeOut,
    FadeIn
  };
  auto getBaseBrightness = []() -> uint8_t
  {
    const int constrainedBrightness = constrain(getLastAppliedBrightness(), 0, 255);
    return static_cast<uint8_t>(constrainedBrightness);
  };
  auto applyTransitionBrightness = [](uint8_t brightness)
  {
    dma_display->setBrightness8(brightness);
    updateGlobalBrightnessScale(brightness);
  };
  auto scaleTransitionBrightness = [](uint8_t baseBrightness, uint8_t level) -> uint8_t
  {
    const uint16_t scaled = static_cast<uint16_t>(baseBrightness) * static_cast<uint16_t>(level);
    return static_cast<uint8_t>(scaled / VIEW_TRANSITION_FADE_STEPS);
  };

  ViewTransitionPhase transitionPhase = ViewTransitionPhase::Idle;
  uint8_t transitionLevel = VIEW_TRANSITION_FADE_STEPS;
  int displayedView = static_cast<int>(currentView);
  int pendingView = displayedView;
#if DEBUG_MODE
  static unsigned long lastSlowFrameLogMs = 0;
#endif

  for (;;)
  {
    const uint32_t taskLoopStartMicros = micros();
    const int requestedView = static_cast<int>(currentView);

    if (sleepModeActive)
    {
      transitionPhase = ViewTransitionPhase::Idle;
      transitionLevel = VIEW_TRANSITION_FADE_STEPS;
      displayedView = requestedView;
      pendingView = requestedView;
      displayCurrentView(requestedView);
      perfTelemetryRecordDuration(PerfDurationId::DisplayTaskBusy, micros() - taskLoopStartMicros);
      vTaskDelayUntil(&lastWake, millisToTicksCeil(sleepFrameInterval));
      continue;
    }

    if (transitionPhase == ViewTransitionPhase::Idle && requestedView != displayedView)
    {
      pendingView = requestedView;
      transitionPhase = ViewTransitionPhase::FadeOut;
      transitionLevel = VIEW_TRANSITION_FADE_STEPS;
    }
    else if (transitionPhase == ViewTransitionPhase::FadeOut)
    {
      // Follow the most recent request while fading out.
      pendingView = requestedView;
    }

    int viewToRender = displayedView;
    if (transitionPhase == ViewTransitionPhase::FadeOut || transitionPhase == ViewTransitionPhase::FadeIn)
    {
      const uint8_t baseBrightness = getBaseBrightness();
      const uint8_t fadedBrightness = scaleTransitionBrightness(baseBrightness, transitionLevel);
      applyTransitionBrightness(fadedBrightness);
    }
    else
    {
      displayedView = requestedView;
      viewToRender = displayedView;
    }

    const uint32_t frameStartMicros = micros();
    {
      PERF_SCOPE(PerfBucket::Render);
      displayCurrentView(viewToRender);
    }

    if (transitionPhase == ViewTransitionPhase::FadeOut)
    {
      if (transitionLevel == 0)
      {
        displayedView = pendingView;
        transitionPhase = ViewTransitionPhase::FadeIn;
        transitionLevel = 0;
      }
      else
      {
        --transitionLevel;
      }
    }
    else if (transitionPhase == ViewTransitionPhase::FadeIn)
    {
      if (transitionLevel >= VIEW_TRANSITION_FADE_STEPS)
      {
        transitionPhase = ViewTransitionPhase::Idle;
        transitionLevel = VIEW_TRANSITION_FADE_STEPS;
        applyTransitionBrightness(getBaseBrightness());
      }
      else
      {
        ++transitionLevel;
      }
    }
    const uint32_t frameMicros = micros() - frameStartMicros;
    unsigned long frameIntervalMs = viewFrameIntervalMillis(viewToRender);
    const uint32_t frameBudgetMicros = viewFrameBudgetMicros(viewToRender);
    perfTelemetryRecordDuration(PerfDurationId::RenderFrame, frameMicros);
    if (frameMicros > frameBudgetMicros)
    {
      perfTelemetryRecordSlowFrame();
#if DEBUG_MODE
      gLastFrameDurationMicros = frameMicros;
      if (frameMicros > gWorstFrameDurationMicros)
      {
        gWorstFrameDurationMicros = frameMicros;
      }
      const unsigned long now = millis();
      if (now - lastSlowFrameLogMs >= 250)
      {
        Serial.printf("Slow frame %lu us (budget %lu us, view %d, worst %lu us)\n",
                      static_cast<unsigned long>(frameMicros),
                      static_cast<unsigned long>(frameBudgetMicros),
                      currentView,
                      static_cast<unsigned long>(gWorstFrameDurationMicros));
        lastSlowFrameLogMs = now;
      }
#endif
    }
#if DEBUG_MODE
    else
    {
      gLastFrameDurationMicros = frameMicros;
    }
#endif
    const unsigned long nowMs = millis();
    if (transitionPhase == ViewTransitionPhase::Idle &&
        !viewNeedsContinuousRefresh(displayedView) &&
        !bluetoothOverlayNeedsContinuousRefresh(nowMs))
    {
      perfTelemetryRecordDuration(PerfDurationId::DisplayTaskBusy, micros() - taskLoopStartMicros);
      ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
      lastWake = xTaskGetTickCount();
      continue;
    }

    if (bleIsOtaBusy() && frameIntervalMs < LF_OTA_ACTIVE_FRAME_INTERVAL_MS)
    {
      frameIntervalMs = LF_OTA_ACTIVE_FRAME_INTERVAL_MS;
    }
    perfTelemetryRecordDuration(PerfDurationId::DisplayTaskBusy, micros() - taskLoopStartMicros);
    vTaskDelayUntil(&lastWake, millisToTicksCeil(frameIntervalMs));
  }
}

// Matrix and Effect Configuration
#define MAX_FPS 45 // Maximum redraw rate, frames/second

#ifndef PDUST_N_COLORS // Allow overriding from a global config if needed
#define PDUST_N_COLORS 8
#endif

#define N_COLORS PDUST_N_COLORS // Number of distinct colors for sand
#define BOX_HEIGHT 4            // Initial height of each color block of sand

// Calculate total number of sand grains.
// Logic: Each color forms a block. There are N_COLORS blocks.
// Width of each block = WIDTH / N_COLORS
// Number of grains per block = (WIDTH / N_COLORS) * BOX_HEIGHT
// Total grains = N_COLORS * (WIDTH / N_COLORS) * BOX_HEIGHT = WIDTH * BOX_HEIGHT
// Original calculation: (BOX_HEIGHT * N_COLORS * 8). If WIDTH=64, N_COLORS=8, BOX_HEIGHT=8,
// this is (8 * 8 * 8) = 512. And WIDTH * BOX_HEIGHT = 64 * 8 = 512. So they are equivalent for these values.
#define N_GRAINS (PANE_WIDTH * BOX_HEIGHT) // Total number of sand grains
// #define N_GRAINS 10
// #include "pixelDustEffect.h" // Include the header for this effect
// #include "main.h"            // Include your main project header if it exists (e.g., for global configs)

// Define global variables declared as extern in the header
uint16_t colors[N_COLORS];
Adafruit_PixelDust sand(PANE_WIDTH, PANE_HEIGHT, N_GRAINS, 1.0f, 128, false); // elasticity 1.0, accel_scale 128
uint32_t prevTime = 0;
static bool pixelDustInitialized = false;

// Initializes the Pixel Dust effect settings
void setupPixelDust()
{
  if (!dma_display)
  {
    Serial.println("PixelDust Error: dma_display is not initialized before setupPixelDust() call!");
    return;
  }
  if (!pixelDustInitialized)
  {
    if (!sand.begin())
    {
      Serial.println("PixelDust Error: sand.begin() failed!");
      return;
    }
    pixelDustInitialized = true;
  }
  if (N_COLORS <= 0)
  {
    Serial.println("PixelDust Error: N_COLORS must be greater than zero.");
    return;
  }

  sand.clear();
  sand.randomize();

  static const uint16_t defaultPalette[] = {
      dma_display->color565(64, 64, 64),
      dma_display->color565(120, 79, 23),
      dma_display->color565(228, 3, 3),
      dma_display->color565(255, 140, 0),
      dma_display->color565(255, 237, 0),
      dma_display->color565(0, 128, 38),
      dma_display->color565(0, 77, 255),
      dma_display->color565(117, 7, 135)};

  const size_t paletteCount = sizeof(defaultPalette) / sizeof(defaultPalette[0]);
  // If N_COLORS exceeds the size of defaultPalette, random colors will be used for the extra entries.
  for (int i = 0; i < N_COLORS; ++i)
  {
    // Use default palette color if available, otherwise generate a random color
    uint16_t color;
    if (i < static_cast<int>(paletteCount))
    {
      color = defaultPalette[i];
    }
    else
    {
      // Use a color wheel to ensure distinct fallback colors
      uint8_t hue = (i * 256 / N_COLORS) % 256;
      CRGB rgb;
      hsv2rgb_rainbow(CHSV(hue, 255, 255), rgb);
      color = dma_display->color565(rgb.r, rgb.g, rgb.b);
    }
    colors[i] = color;
  }
}

// Add an enum for clarity (optional, but good practice)
enum SpiralColorMode
{
  SPIRAL_COLOR_PALETTE,
  SPIRAL_COLOR_WHITE
};

// Modified function signature:
void updateAndDrawFullScreenSpiral(SpiralColorMode colorMode)
{ // Added colorMode parameter
  unsigned long currentTime = millis();
  if (currentTime - lastFullScreenSpiralUpdateTime < FULL_SCREEN_SPIRAL_FRAME_INTERVAL_MS)
  {
    return;
  }
  lastFullScreenSpiralUpdateTime = currentTime;

  // Update animation parameters (rotation, color scroll - color scroll only if palette mode)
  fullScreenSpiralAngle += FULL_SPIRAL_ROTATION_SPEED_RAD_PER_UPDATE;
  if (fullScreenSpiralAngle >= TWO_PI)
    fullScreenSpiralAngle -= TWO_PI;

  if (colorMode == SPIRAL_COLOR_PALETTE)
  { // Only update color offset if using palette
    fullScreenSpiralColorOffset += FULL_SPIRAL_COLOR_SCROLL_SPEED;
  }

  const int centerX = PANE_WIDTH / 2;
  const int centerY = PANE_HEIGHT / 2;
  const float maxRadiusSquared = powf(hypotf(PANE_WIDTH / 2.0f, PANE_HEIGHT / 2.0f) * 1.1f, 2.0f);
  const float deltaTheta = 0.05f;
  const float growthFactor = expf(LOG_SPIRAL_B_COEFF * deltaTheta);
  const float rotCos = cosf(deltaTheta);
  const float rotSin = sinf(deltaTheta);

  float radius = LOG_SPIRAL_A_COEFF;
  float sinAngle = sinf(fullScreenSpiralAngle);
  float cosAngle = cosf(fullScreenSpiralAngle);

  float x = radius * cosAngle;
  float y = radius * sinAngle;

  float colorPhase = fullScreenSpiralColorOffset;
  const float colorPhaseStep = deltaTheta * SPIRAL_ARM_COLOR_FACTOR;

  const int maxSteps = 512;
  for (int step = 0; step < maxSteps; ++step)
  {
    const int drawX = static_cast<int>(centerX + x + 0.5f);
    const int drawY = static_cast<int>(centerY + y + 0.5f);

    uint16_t pixel_color;
    if (colorMode == SPIRAL_COLOR_WHITE)
    {
      pixel_color = dma_display->color565(255, 255, 255);
    }
    else
    {
      const uint8_t color_index = static_cast<uint8_t>(colorPhase);
      CRGB crgb_color = ColorFromPalette(RainbowColors_p, color_index, 255, LINEARBLEND);
      pixel_color = dma_display->color565(crgb_color.r, crgb_color.g, crgb_color.b);
    }

    if (SPIRAL_THICKNESS_RADIUS == 0)
    {
      if (drawX >= 0 && drawX < PANE_WIDTH && drawY >= 0 && drawY < PANE_HEIGHT)
      {
        dma_display->drawPixel(drawX, drawY, pixel_color);
      }
    }
    else
    {
      const int side_length = SPIRAL_THICKNESS_RADIUS * 2 + 1;
      const int start_x = drawX - SPIRAL_THICKNESS_RADIUS;
      const int start_y = drawY - SPIRAL_THICKNESS_RADIUS;
      dma_display->fillRect(start_x, start_y, side_length, side_length, pixel_color);
    }

    colorPhase += colorPhaseStep;

    const float rotatedX = x * rotCos - y * rotSin;
    const float rotatedY = x * rotSin + y * rotCos;
    x = rotatedX * growthFactor;
    y = rotatedY * growthFactor;

    if ((x * x + y * y) > maxRadiusSquared)
    {
      break;
    }
  }
}

void setup()
{
  Serial.begin(BAUD_RATE);

  // Native USB CDC on the ESP32-S3 can take a moment to attach after reset.
  // Wait briefly so startup logs are visible in the serial monitor, but do not
  // stall boot indefinitely if no host is connected.
  const unsigned long serialAttachStartMs = millis();
  while (!Serial && (millis() - serialAttachStartMs) < 2500UL)
  {
    delay(10);
  }
  delay(50);

#if PERF_SELF_TEST
  runPerfSelfTest();
#endif

  Serial.println(" ");
  Serial.println(" ");
  Serial.println("*****************************************************");
  Serial.println("*****************************************************");
  Serial.println("*****************************************************");
  Serial.println("*******************    LumiFur    *******************");
  Serial.println("*****************************************************");
  Serial.println("*****************************************************");
  Serial.println("*****************************************************");
  Serial.println(" ");
  Serial.println(" ");

  updateGlobalBrightnessScale(userBrightness);

#if DEBUG_MODE
  Serial.println("DEBUG MODE ENABLED");
  Serial.printf("Firmware: %s\n", FIRMWARE_VERSION);
  Serial.printf("Commit: %s\n", GIT_COMMIT);
  Serial.printf("Branch: %s\n", GIT_BRANCH);
  Serial.printf("Build: %s %s\n", BUILD_DATE, BUILD_TIME);
#endif

  gVideoStorageReady = initializeVideoStorage();

  initTempSensor(); // Initialize Temperature Sensor

  initPreferences(); // Initialize Preferences
  userBrightness = static_cast<uint8_t>(constrain(getUserBrightness(), 0, 255));
  sliderBrightness = map(userBrightness, 1, 255, 1, 100);
  autoBrightnessEnabled = getAutoBrightness();
  syncBrightnessState(userBrightness);
  accelerometerEnabled = getAccelerometerEnabled();
  sleepModeEnabled = getSleepMode();
  auroraModeEnabled = getAuroraMode();
  staticColorModeEnabled = getStaticColorMode();
  if (staticColorModeEnabled)
  {
    auroraModeEnabled = false;
  }
  ensureStaticColorLoaded();
  {
    String storedText = getUserText();
    if (storedText.length() > 0)
    {
      size_t copyLen = storedText.length();
      if (copyLen >= sizeof(txt))
      {
        copyLen = sizeof(txt) - 1;
      }
      memcpy(txt, storedText.c_str(), copyLen);
      txt[copyLen] = '\0';
      auto &scrollState = scroll::state();
      scrollState.textInitialized = false;
    }
    else
    {
      txt[0] = '\0';
    }

    auto &scrollState = scroll::state();
    scrollState.speedSetting = getScrollSpeed();
    scroll::updateIntervalFromSpeed();
  }

  // - LOAD LAST VIEW -
  currentView = getLastView();
  previousView = currentView;
  gLastPersistedView = currentView;
  Serial.printf("Loaded last view: %d\n", currentView);
  ensureStrobeSettingsLoaded();

  // Retrieve and print stored brightness value.
  // int userBrightness = getUserBrightness();
  // Map userBrightness (1-100) to hardware brightness (1-255).
  // int hwBrightness = map(userBrightness, 1, 100, 1, 255);
  Serial.printf("Stored brightness: %d\n", userBrightness);

  /*
  // --- Initialize Scrolling Text ---
    dma_display->setFont(&FreeSans9pt7b); // Set font to measure text width
    dma_display->setTextSize(1);
    strcpy(txt, "LumiFur Controller - FW v" FIRMWARE_VERSION);
    int16_t x1, y1;
    uint16_t w, h;
    dma_display->getTextBounds(txt, 0, 0, &x1, &y1, &w, &h);
    textMin = -w;
    textX = dma_display->width();
    textY = (dma_display->height() - h) / 2 + h; // Center vertically
  */

  ///////////////////// Setup BLE ////////////////////////
  Serial.println("Initializing BLE...");
  // NimBLEDevice::init("LumiFur_Controller");
  NimBLEDevice::init("LF-052618");
  // NimBLEDevice::setPower(ESP_PWR_LVL_P9); // Power level 9 (highest) for best range
  // NimBLEDevice::setPower(ESP_PWR_LVL_P21, NimBLETxPowerType::All); // Power level 21 (highest) for best range
  NimBLEDevice::setPower(ESP_PWR_LVL_P9, NimBLETxPowerType::All); // Power level 21 (highest) for best range
  // NimBLEDevice::setSecurityAuth(BLE_SM_PAIR_AUTHREQ_BOND | BLE_SM_PAIR_AUTHREQ_SC);
  NimBLEDevice::setSecurityAuth(true, true, true);
  // Keep default passkey so onPassKeyDisplay supplies a dynamic code.
  NimBLEDevice::setSecurityPasskey(123456);
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_ONLY);
  pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(&serverCallbacks);
  pServer->advertiseOnDisconnect(false);

  NimBLEService *pService = pServer->createService(SERVICE_UUID);

  NimBLECharacteristic *pDeviceInfoCharacteristic = pService->createCharacteristic(
      INFO_CHARACTERISTIC_UUID,
      NIMBLE_PROPERTY::READ |
          NIMBLE_PROPERTY::NOTIFY);

  // Construct JSON string
  std::string jsonInfo = std::string("{") +
                         "\"fw\":\"" + std::string(FIRMWARE_VERSION) + "\"," +
                         "\"commit\":\"" + std::string(GIT_COMMIT) + "\"," +
                         "\"branch\":\"" + std::string(GIT_BRANCH) + "\"," +
                         "\"build\":\"" + std::string(BUILD_DATE) + " " + BUILD_TIME + "\"," +
                         "\"model\":\"" + std::string(DEVICE_MODEL) + "\"," +
                         "\"compat\":\"" + std::string(APP_COMPAT_VERSION) + "\"," +
                         "\"id\":\"" + NimBLEDevice::getAddress().toString() + "\"" +
                         "}";

  pDeviceInfoCharacteristic->setValue(jsonInfo.c_str());
  Serial.println("Device Info Service started");
  Serial.println(jsonInfo.c_str());

  // Face control characteristic with encryption
  pFaceCharacteristic = pService->createCharacteristic(
      CHARACTERISTIC_UUID,
      NIMBLE_PROPERTY::READ |
          NIMBLE_PROPERTY::WRITE |
          NIMBLE_PROPERTY::WRITE_ENC |
          NIMBLE_PROPERTY::NOTIFY
      // NIMBLE_PROPERTY::READ_ENC  // only allow reading if paired / encrypted
      // NIMBLE_PROPERTY::WRITE_ENC  // only allow writing if paired / encrypted
  );

  // Set initial view value
  uint8_t bleViewValue = static_cast<uint8_t>(currentView);
  pFaceCharacteristic->setValue(&bleViewValue, 1);
  pFaceCharacteristic->setCallbacks(&chrCallbacks);

  pCommandCharacteristic = pService->createCharacteristic(
      COMMAND_CHARACTERISTIC_UUID,
      NIMBLE_PROPERTY::WRITE |
          NIMBLE_PROPERTY::WRITE_ENC |
          NIMBLE_PROPERTY::NOTIFY);
  pCommandCharacteristic->setCallbacks(&cmdCallbacks);
  Serial.print("Command Characteristic created, UUID: ");
  Serial.println(pCommandCharacteristic->getUUID().toString().c_str());
  Serial.print("Properties: ");
  Serial.println(pCommandCharacteristic->getProperties()); // Print properties as integer

  // Temperature characteristic with encryption
  pTemperatureCharacteristic =
      pService->createCharacteristic(
          TEMPERATURE_CHARACTERISTIC_UUID,
          NIMBLE_PROPERTY::WRITE |
              NIMBLE_PROPERTY::NOTIFY
          // NIMBLE_PROPERTY::READ_ENC
      );

  // Create a characteristic for configuration.
  pConfigCharacteristic = pService->createCharacteristic(
      CONFIG_CHARACTERISTIC_UUID,
      NIMBLE_PROPERTY::READ |
          NIMBLE_PROPERTY::WRITE |
          NIMBLE_PROPERTY::WRITE_ENC |
          // NIMBLE_PROPERTY::WRITE_NR |
          NIMBLE_PROPERTY::NOTIFY);

  // Set the callback to handle writes.
  pConfigCharacteristic->setCallbacks(&configCallbacks);

  // Optionally, set an initial value.
  uint8_t initValue[5] = {
      static_cast<uint8_t>(autoBrightnessEnabled ? 1 : 0),
      static_cast<uint8_t>(accelerometerEnabled ? 1 : 0),
      static_cast<uint8_t>(sleepModeEnabled ? 1 : 0),
      static_cast<uint8_t>(auroraModeEnabled ? 1 : 0),
      static_cast<uint8_t>(staticColorModeEnabled ? 1 : 0)};

  pConfigCharacteristic->setValue(initValue, sizeof(initValue));
  if (pConfigCharacteristic != nullptr)
  {
    Serial.println("Config Characteristic created");
  }
  else
  {
    Serial.println("Error: Failed to create config characteristic.");
  }

  //
  pTemperatureLogsCharacteristic = pService->createCharacteristic(
      TEMPERATURE_LOGS_CHARACTERISTIC_UUID,
      NIMBLE_PROPERTY::NOTIFY);
  // pTemperatureLogsCharacteristic->setCallbacks(&chrCallbacks);
  Serial.println("Command Characteristic created");
  pTemperatureLogsCharacteristic->setCallbacks(&logsCallbacks); // If using new logsCallbacks
  Serial.print("Temperature Logs Characteristic created, UUID: ");
  Serial.println(pTemperatureLogsCharacteristic->getUUID().toString().c_str());
  Serial.print("Properties: ");
  Serial.println(pTemperatureLogsCharacteristic->getProperties()); // Print properties as integer

  // Set up descriptors
  NimBLE2904 *faceDesc = pFaceCharacteristic->create2904();
  faceDesc->setFormat(NimBLE2904::FORMAT_UINT8);
  faceDesc->setUnit(0x2700); // Unit-less number
  faceDesc->setCallbacks(&dscCallbacks);
  // Add user description descriptor
  NimBLEDescriptor *pDesc =
      pFaceCharacteristic->createDescriptor(
          "2901",
          NIMBLE_PROPERTY::READ,
          20);
  pDesc->setValue("Face Control");

  // Assuming pTemperatureCharacteristic is already created:
  NimBLEDescriptor *tempDesc =
      pTemperatureCharacteristic->createDescriptor(
          "2901", // Standard UUID for the Characteristic User Description
          NIMBLE_PROPERTY::READ,
          20 // Maximum length for the descriptor's value
      );
  tempDesc->setValue("Temperature Sensor");
  NimBLE2904 *tempFormat = pTemperatureCharacteristic->create2904();
  tempFormat->setFormat(NimBLE2904::FORMAT_UINT8);
  tempFormat->setUnit(0x272F); // For example, 0x272F might represent degrees Celsius per specific BLE specifications

  pBrightnessCharacteristic = pService->createCharacteristic(
      BRIGHTNESS_CHARACTERISTIC_UUID,
      NIMBLE_PROPERTY::READ |
          // NIMBLE_PROPERTY::NOTIFY |
          NIMBLE_PROPERTY::WRITE |
          NIMBLE_PROPERTY::WRITE_ENC |
          NIMBLE_PROPERTY::WRITE_NR);
  pBrightnessCharacteristic->setCallbacks(&brightnessCallbacks);
  // initialize with current brightness

  pOtaCharacteristic = pService->createCharacteristic(
      OTA_CHARACTERISTIC_UUID,
      NIMBLE_PROPERTY::READ |
          NIMBLE_PROPERTY::WRITE |
          NIMBLE_PROPERTY::WRITE_ENC |
          NIMBLE_PROPERTY::WRITE_NR |
          NIMBLE_PROPERTY::NOTIFY);
  pOtaCharacteristic->setCallbacks(&otaCallbacks);
  NimBLEDescriptor *otaDesc = pOtaCharacteristic->createDescriptor(
      "2901",
      NIMBLE_PROPERTY::READ,
      20);
  otaDesc->setValue("OTA Control");
  pBrightnessCharacteristic->setValue(&userBrightness, 1);

  setupAdaptiveBrightness();

  // Create lux characteristic (read-only with notifications)
  pLuxCharacteristic = pService->createCharacteristic(
      LUX_CHARACTERISTIC_UUID,
      NIMBLE_PROPERTY::READ |
          NIMBLE_PROPERTY::NOTIFY);

  // Initialize with current lux value
  uint16_t initialLux = getAmbientLuxU16();
  uint8_t luxBytes[2] = {
      static_cast<uint8_t>(initialLux & 0xFF),
      static_cast<uint8_t>((initialLux >> 8) & 0xFF)};
  pLuxCharacteristic->setValue(luxBytes, 2);
  // Add descriptor for lux characteristic
  NimBLEDescriptor *luxDesc = pLuxCharacteristic->createDescriptor(
      "2901", // Standard UUID for Characteristic User Description
      NIMBLE_PROPERTY::READ,
      20 // Maximum length for the descriptor's value
  );
  luxDesc->setValue("Ambient Light Sensor");
  NimBLE2904 *luxFormat = pLuxCharacteristic->create2904();
  luxFormat->setFormat(NimBLE2904::FORMAT_UINT16);
  luxFormat->setUnit(0x2731); // 0x2731 is the standard unit for Illuminance (lux)

  pScrollTextCharacteristic = pService->createCharacteristic(
      SCROLL_TEXT_CHARACTERISTIC_UUID,
      NIMBLE_PROPERTY::READ |
          NIMBLE_PROPERTY::WRITE |
          NIMBLE_PROPERTY::WRITE_ENC | // Require encryption for writes.
          NIMBLE_PROPERTY::NOTIFY);
  pScrollTextCharacteristic->setCallbacks(&scrollTextCallbacks);

  // Set initial value (current text)
  pScrollTextCharacteristic->setValue(txt);

  // Add descriptor
  NimBLEDescriptor *scrollTextDesc = pScrollTextCharacteristic->createDescriptor(
      "2901",
      NIMBLE_PROPERTY::READ,
      20);
  scrollTextDesc->setValue("Scrolling Text");

  Serial.println("Scroll Text Characteristic created");

  pStaticColorCharacteristic = pService->createCharacteristic(
      STATIC_COLOR_CHARACTERISTIC_UUID,
      NIMBLE_PROPERTY::READ |
          NIMBLE_PROPERTY::READ_ENC |
          NIMBLE_PROPERTY::WRITE |
          NIMBLE_PROPERTY::WRITE_ENC |
          NIMBLE_PROPERTY::WRITE_NR |
          NIMBLE_PROPERTY::NOTIFY);
  pStaticColorCharacteristic->setCallbacks(&staticColorCallbacks);
  String initialColorHex = getStaticColorHexString();
  pStaticColorCharacteristic->setValue(initialColorHex.c_str());
  NimBLEDescriptor *staticColorDesc = pStaticColorCharacteristic->createDescriptor(
      "2901",
      NIMBLE_PROPERTY::READ,
      20);
  staticColorDesc->setValue("Static Color");
  Serial.println("Static Color Characteristic created");

  pStrobeSettingsCharacteristic = pService->createCharacteristic(
      STROBE_SETTINGS_CHARACTERISTIC_UUID,
      NIMBLE_PROPERTY::READ |
          NIMBLE_PROPERTY::READ_ENC |
          NIMBLE_PROPERTY::WRITE |
          NIMBLE_PROPERTY::WRITE_ENC |
          NIMBLE_PROPERTY::WRITE_NR |
          NIMBLE_PROPERTY::NOTIFY);
  pStrobeSettingsCharacteristic->setCallbacks(&strobeSettingsCallbacks);
  const std::string initialStrobeSettings = buildStrobeSettingsPayload();
  pStrobeSettingsCharacteristic->setValue(initialStrobeSettings);
  NimBLEDescriptor *strobeSettingsDesc = pStrobeSettingsCharacteristic->createDescriptor(
      "2901",
      NIMBLE_PROPERTY::READ,
      20);
  strobeSettingsDesc->setValue("Strobe Settings");
  Serial.println("Strobe Settings Characteristic created");

  // nimBLEService* pBaadService = pServer->createService("BAAD");
  // Services are started when the GATT server starts during advertising.

  /** Create an advertising instance and add the services to the advertised data */
  NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->setName("LumiFur_Controller");
  // pAdvertising->setAppearance(0);         // Appearance value (0 is generic)
  pAdvertising->enableScanResponse(true); // Enable scan response to include more data
  pAdvertising->addServiceUUID(pService->getUUID());
  refreshBleAdvertising();

  Serial.println(shouldBleAdvertise() ? "BLE setup complete - advertising started"
                                      : "BLE setup complete - waiting for pairing mode");

  // Redefine pins if required
  // HUB75_I2S_CFG::i2s_pins _pins={R1, G1, BL1, R2, G2, BL2, CH_A, CH_B, CH_C, CH_D, CH_E, LAT, OE, CLK};
  // HUB75_I2S_CFG mxconfig(PANEL_WIDTH, PANEL_HEIGHT, PANELS_NUMBER);
  HUB75_I2S_CFG::i2s_pins _pins = {R1_PIN, G1_PIN, B1_PIN, R2_PIN, G2_PIN, B2_PIN, A_PIN, B_PIN, C_PIN, D_PIN, E_PIN, LAT_PIN, OE_PIN, CLK_PIN};

  // Module configuration
  HUB75_I2S_CFG mxconfig(
      PANEL_WIDTH,   // module width
      PANEL_HEIGHT,  // module height
      PANELS_NUMBER, // Chain length
      _pins          // Pin mapping
  );

  mxconfig.gpio.e = PIN_E;
  mxconfig.driver = HUB75_I2S_CFG::FM6126A; // for panels using FM6126A chips
  mxconfig.i2sspeed = HUB75_I2S_CFG::HZ_16M;                // 20 MHz proved unstable on this panel; keep the highest stable clock.
  mxconfig.min_refresh_rate = LF_HUB75_MIN_REFRESH_RATE_HZ; // Favor refresh stability over extra effective color depth.
  mxconfig.latch_blanking = LF_HUB75_LATCH_BLANKING;        // Keep blanking explicit to avoid per-panel surprises.
  mxconfig.clkphase = LF_HUB75_CLKPHASE;                    // false selects the library's negative-edge clocking mode.
  mxconfig.double_buff = true;
  mxconfig.setPixelColorDepthBits(LF_HUB75_COLOR_DEPTH_BITS);

#ifndef VIRTUAL_PANE
  dma_display = new MatrixPanel_I2S_DMA(mxconfig);
  dma_display->begin();
  dma_display->setBrightness8(userBrightness);
  updateGlobalBrightnessScale(userBrightness);
  syncBrightnessState(userBrightness);
  initFlameEffect(dma_display);
#else
  chain = new MatrixPanel_I2S_DMA(mxconfig);
  chain->begin();
  chain->setBrightness8(userBrightness);
  updateGlobalBrightnessScale(userBrightness);
  syncBrightnessState(userBrightness);
  // create VirtualDisplay object based on our newly created dma_display object
  matrix = new VirtualMatrixPanel((*chain), NUM_ROWS, NUM_COLS, PANEL_WIDTH, PANEL_HEIGHT, CHAIN_TOP_LEFT_DOWN);
  initFlameEffect(matrix);
#endif

  dma_display->clearScreen();
  dma_display->flipDMABuffer();

#if defined(ARDUINO_ADAFRUIT_MATRIXPORTAL_ESP32S3)
  // Initialize onboard components for MatrixPortal ESP32-S3
  statusPixel.begin(); // Initialize NeoPixel status light

  if (!accel.begin(0x19))
  { // Default I2C address for LIS3DH on MatrixPortal
    Serial.println("Could not find accelerometer, check wiring!");
    err(250);                            // Fast blink = I2C error
    g_accelerometer_initialized = false; // Set flag
  }
  else
  {
    Serial.println("Accelerometer found!");
    accel.setRange(LIS3DH_RANGE_4_G); // 4G range is sufficient for motion detection
    accel.setDataRate(LIS3DH_DATARATE_50_HZ);
    accel.setPerformanceMode(LIS3DH_MODE_LOW_POWER);
    g_accelerometer_initialized = true; // Set flag on success // Set to low power mode
  }
#endif

  // setupPixelDust();

#if defined(ARDUINO_ADAFRUIT_MATRIXPORTAL_ESP32S3) || defined(ACCEL_AVAILABLE)
  // Ensure dma_display, accel, and accelerometerEnabled are initialized before this line
  fluidEffectInstance = new FluidEffect(dma_display, &accel, &accelerometerEnabled);
#else
  // Pass nullptr for accelerometer if not available/compiled
  fluidEffectInstance = new FluidEffect(dma_display, nullptr, &accelerometerEnabled);
#endif

  if (!fluidEffectInstance)
  {
    Serial.println("FATAL: Failed to allocate FluidEffect instance!");
    // Handle error appropriately, e.g., by blinking an LED or halting.
    // For now, it will just print "Fluid Error" in displayCurrentView if null.
  }

  videoPlayerInstance = new MonoVideoPlayer(FFat, BAD_APPLE_VIDEO_PATH, PANE_WIDTH, PANE_HEIGHT, PANEL_WIDTH, DEBUG_VIDEO_PLAYER != 0);
  if (!videoPlayerInstance)
  {
    Serial.println("FATAL: Failed to allocate MonoVideoPlayer instance!");
  }
  else if (!gVideoStorageReady)
  {
    Serial.println("Video player created; waiting for FFat storage to become available.");
  }

  // ... rest of your setup() code ...
  // e.g., currentView = getLastView();

  // If the initial view is the fluid effect, call begin()
  if (currentView == VIEW_FLUID_EFFECT && fluidEffectInstance)
  { // Assuming 16 is the fluid effect view
    fluidEffectInstance->begin();
  }
  if (currentView == VIEW_VIDEO_PLAYER && videoPlayerInstance)
  {
    videoPlayerInstance->begin();
  }

  lastActivityTime = millis(); // Initialize the activity timer for sleep mode

  randomSeed(analogRead(0)); // Seed the random number generator

  // Set sleep timeout based on debug mode
  SLEEP_TIMEOUT_MS = DEBUG_MODE ? SLEEP_TIMEOUT_MS_DEBUG : SLEEP_TIMEOUT_MS_NORMAL;

  // Set initial plasma color palette
  currentPalette = RainbowColors_p;

  snprintf(txt, sizeof(txt), "%s", getUserText().c_str());
  initializeScrollingText();

  ////////Setup Bouncing Squares////////
  myDARK = dma_display->color565(64, 64, 64);
  myWHITE = dma_display->color565(192, 192, 192);
  myRED = dma_display->color565(255, 0, 0);
  myGREEN = dma_display->color565(0, 255, 0);
  myBLUE = dma_display->color565(0, 0, 255);

  colours = {{myDARK, myWHITE, myRED, myGREEN, myBLUE}};

  // Create some random squares
  for (int i = 0; i < numSquares; i++)
  {
    Squares[i].square_size = random(2, 10);
    Squares[i].xpos = random(0, dma_display->width() - Squares[i].square_size);
    Squares[i].ypos = random(0, dma_display->height() - Squares[i].square_size);
    Squares[i].velocityx = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
    Squares[i].velocityy = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);

    int random_num = random(6);
    Squares[i].colour = colours[random_num];
  }

  // Initialize the starfield
  initStarfieldAnimation();
  initDVDLogoAnimation();

  /* ----------------------------------------------------------------------
  Use internal pull-up resistor, as the up and down buttons
  do not have any pull-up resistors connected to them
  and pressing either of them pulls the input low.
  ------------------------------------------------------------------------- */

  micInit();

#ifdef BUTTON_UP
  pinMode(BUTTON_UP, INPUT_PULLUP);
#endif
#ifdef BUTTON_DOWN
  pinMode(BUTTON_DOWN, INPUT_PULLUP);
#endif

  // uint8_t wheelval = 0; // Wheel value for color cycling

  //  #if DEBUG_MODE
  while (loadingProgress <= loadingMax)
  {
    // Boot screen
    dma_display->clearScreen();
    dma_display->setTextSize(2);
    dma_display->setTextColor(dma_display->color565(255, 0, 255));
    dma_display->setCursor(0, 10);
    dma_display->print("LumiFur");
    dma_display->setCursor(64, 10);
    dma_display->print("LumiFur");
    dma_display->setTextSize(1);
    dma_display->setCursor(0, 20);
    dma_display->setTextColor(dma_display->color565(255, 255, 255));
    dma_display->setFont(&TomThumb); // Set font
    dma_display->print("Booting...");
    dma_display->setCursor(64, 20);
    dma_display->print("Booting...");
    displayLoadingBar();
    dma_display->flipDMABuffer();
    loadingProgress++;
    delay(15); // ~1.5 s total at loadingMax=100
  }
  // #endif

  // Spawn BLE notify task (you already have):
  bleWorkQueue = xQueueCreate(LF_BLE_WORKER_QUEUE_LENGTH, sizeof(BleWorkItem));
  if (bleWorkQueue == nullptr)
  {
    Serial.println("Failed to create BLE worker queue");
    err(125);
  }

  xTaskCreatePinnedToCore(
      bleNotifyTask,
      "BLE Task",
      4096,
      NULL,
      1,
      &bleNotifyTaskHandle,
      CONFIG_BT_NIMBLE_PINNED_TO_CORE);

  xTaskCreatePinnedToCore(
      bleWorkerTask,
      "BLE Worker",
      LF_BLE_WORKER_STACK_SIZE,
      NULL,
      LF_BLE_WORKER_PRIORITY,
      &bleWorkerTaskHandle,
      CONFIG_BT_NIMBLE_PINNED_TO_CORE);

  // Now spawn the display task pinned to the other core so rendering never competes with loop():

  xTaskCreatePinnedToCore(
      displayTask,
      "Display Task",
      16384, // stack bytes
      NULL,
      3, // priority
      &displayTaskHandle,
      0 // pin to core 0
  );

  // Then return—loop() can be left empty or just do background housekeeping

  // create a 1 ms auto‑reloading FreeRTOS timer
}

void drawDVDLogo(int x, int y, uint16_t color)
{
  struct DvdLogoSpan
  {
    uint8_t x;
    uint8_t length;
  };
  struct DvdLogoRow
  {
    uint8_t count = 0;
    DvdLogoSpan spans[(dvdWidth + 1) / 2] = {};
  };

  static bool spansInitialized = false;
  static DvdLogoRow rows[dvdHeight];

  if (!spansInitialized)
  {
    const int byteWidth = (dvdWidth + 7) >> 3;
    for (int row = 0; row < dvdHeight; ++row)
    {
      DvdLogoRow &rowData = rows[row];
      bool runActive = false;
      uint8_t runStart = 0;
      uint8_t runLength = 0;

      for (int pixel = 0; pixel < dvdWidth; ++pixel)
      {
        const int byteIndex = row * byteWidth + (pixel >> 3);
        const uint8_t rowByte = pgm_read_byte(&DvDLogo[byteIndex]);
        const bool pixelSet = (rowByte & static_cast<uint8_t>(0x80U >> (pixel & 7))) != 0;

        if (pixelSet)
        {
          if (!runActive)
          {
            runActive = true;
            runStart = static_cast<uint8_t>(pixel);
            runLength = 1;
          }
          else
          {
            ++runLength;
          }
        }
        else if (runActive)
        {
          DvdLogoSpan &span = rowData.spans[rowData.count++];
          span.x = runStart;
          span.length = runLength;
          runActive = false;
        }
      }

      if (runActive)
      {
        DvdLogoSpan &span = rowData.spans[rowData.count++];
        span.x = runStart;
        span.length = runLength;
      }
    }

    spansInitialized = true;
  }

  for (int row = 0; row < dvdHeight; ++row)
  {
    const DvdLogoRow &rowData = rows[row];
    for (uint8_t spanIndex = 0; spanIndex < rowData.count; ++spanIndex)
    {
      const DvdLogoSpan &span = rowData.spans[spanIndex];
      dma_display->drawFastHLine(x + span.x, y + row, span.length, color);
    }
  }
}

void updateDVDLogos()
{
  const unsigned long now = millis();
  const bool shouldAdvance = (now - lastDvdUpdate) >= dvdUpdateInterval;
  if (shouldAdvance)
  {
    lastDvdUpdate = now;
  }

  for (int i = 0; i < 2; i++)
  {
    DVDLogo &logo = logos[i];
    int minX = (i == 0) ? 0 : 64;
    int maxX = minX + 64;

    if (shouldAdvance)
    {
      logo.x += logo.vx;
      logo.y += logo.vy;

      bool bounced = false;

      if (logo.x < minX)
      {
        logo.x = minX;
        logo.vx = -logo.vx;
        bounced = true;
      }
      else if (logo.x + dvdWidth > maxX)
      {
        logo.x = maxX - dvdWidth;
        logo.vx = -logo.vx;
        bounced = true;
      }

      if (logo.y < 0)
      {
        logo.y = 0;
        logo.vy = -logo.vy;
        bounced = true;
      }
      else if (logo.y + dvdHeight > dma_display->height())
      {
        logo.y = dma_display->height() - dvdHeight;
        logo.vy = -logo.vy;
        bounced = true;
      }

      if (bounced)
      {
        logo.color = dma_display->color565(random(256), random(256), random(256));
      }
    }

    drawDVDLogo(logo.x, logo.y, logo.color);
  }
  // dma_display->flipDMABuffer(); // Flip the buffer to show the updated logos
  // Rendering cadence is handled by displayTask; avoid delaying here.
}

void displayCurrentMaw()
{
  const uint8_t mawBrightness = micGetMouthBrightness();
  mouthOpen = micIsMouthOpen();
  // build a gray-scale color from current mic level
  uint16_t col = dma_display->color565(mawBrightness,
                                       mawBrightness,
                                       mawBrightness);
  // Draw open or closed based on the mic‐triggered flag
  if (mouthOpen)
  {
#if DEBUG_MODE
    Serial.println("Displaying open maw");
#endif
    drawXbm565(0, 10, 64, 22, maw2, col);
    drawXbm565(64, 10, 64, 22, maw2L, col);
  }
  else
  {
#if DEBUG_MODE
    Serial.println("Displaying Closed maw");
#endif
    drawXbm565(0, 10, 64, 22, maw2Closed, col);
    drawXbm565(64, 10, 64, 22, maw2ClosedL, col);
  }
}
// Runs one frame of the Pixel Dust animation
bool getLatestAcceleration(float &x, float &y, float &z, const unsigned long sampleInterval);
void PixelDustEffect()
{
  static uint32_t lastFrameMicros = 0;

  const uint32_t now = micros();
  if ((now - lastFrameMicros) < (1000000L / MAX_FPS))
  {
    return;
  }
  lastFrameMicros = now;

  if (!pixelDustInitialized)
  {
    setupPixelDust();
    if (!pixelDustInitialized)
    {
      return;
    }
  }

  if (!dma_display)
  {
    return;
  }

  int16_t ax_scaled = 0;
  int16_t ay_scaled = static_cast<int16_t>(0.1f * 128.0f);

  float accelX = 0.0f;
  float accelY = 0.0f;
  float accelZ = 0.0f;

  if (getLatestAcceleration(accelX, accelY, accelZ, MOTION_SAMPLE_INTERVAL_FAST))
  {
    ax_scaled = static_cast<int16_t>(accelX * 1000.0f);
    ay_scaled = static_cast<int16_t>(accelY * 1000.0f);
  }

  sand.iterate(ax_scaled, ay_scaled, 0);

  dimension_t px, py;
  for (grain_count_t i = 0; i < N_GRAINS; ++i)
  {
    sand.getPosition(i, &px, &py);
    int color_index = 0;
    if (N_COLORS > 0)
    {
      color_index = static_cast<int>((static_cast<uint32_t>(px) * N_COLORS) / PANE_WIDTH);
      if (color_index >= N_COLORS)
      {
        color_index = N_COLORS - 1;
      }
    }
    const int px_i = static_cast<int>(px);
    const int py_i = static_cast<int>(py);
    if (px_i >= 0 && py_i >= 0 && px_i < PANE_WIDTH && py_i < PANE_HEIGHT)
    {
      dma_display->drawPixel(px_i, py_i, colors[color_index]);
    }
  }
}

using ViewRenderFunc = void (*)();

static void renderDebugSquaresView()
{
  for (int i = 0; i < numSquares; i++)
  {
    dma_display->fillRect(Squares[i].xpos, Squares[i].ypos, Squares[i].square_size, Squares[i].square_size, Squares[i].colour);

    if (Squares[i].square_size + Squares[i].xpos >= dma_display->width())
    {
      Squares[i].velocityx *= -1;
    }
    else if (Squares[i].xpos <= 0)
    {
      Squares[i].velocityx = abs(Squares[i].velocityx);
    }

    if (Squares[i].square_size + Squares[i].ypos >= dma_display->height())
    {
      Squares[i].velocityy *= -1;
    }
    else if (Squares[i].ypos <= 0)
    {
      Squares[i].velocityy = abs(Squares[i].velocityy);
    }

    Squares[i].xpos += Squares[i].velocityx;
    Squares[i].ypos += Squares[i].velocityy;
  }
}

static void renderLoadingBarView()
{
  drawXbm565(23, 2, 18, 21, appleLogoApple_logo_black, dma_display->color565(255, 255, 255));
  drawXbm565(88, 2, 18, 21, appleLogoApple_logo_black, dma_display->color565(255, 255, 255));

  displayLoadingBar();
  loadingProgress++;
  if (loadingProgress > loadingMax)
  {
    loadingProgress = 0;
  }
}

static void renderFaceWithPlasma()
{
  baseFace();
  updatePlasmaFace();
}

static void renderSpiralEyesView()
{
  updatePlasmaFace();
  baseFace();
  updateRotatingSpiral();
}

static void renderPlasmaFaceView()
{
  drawPlasmaFace();
  updatePlasmaFace();
}

static void renderBsodView()
{
  dma_display->fillScreen(dma_display->color565(0, 0, 255));
  dma_display->setTextColor(dma_display->color565(255, 255, 255));
  dma_display->setTextSize(2);
  dma_display->setCursor(5, 15);
  dma_display->print(":(");
  dma_display->setCursor(69, 15);
  dma_display->print(":(");
}

static void renderFlameEffectView()
{
  updateAndDrawFlameEffect();
}

static void renderFluidEffectView()
{
  if (fluidEffectInstance)
  {
    fluidEffectInstance->updateAndDraw();
  }
  else
  {
    dma_display->fillRect(0, 0, dma_display->width(), dma_display->height(), 0);
    dma_display->setFont(&TomThumb);
    dma_display->setTextSize(1);
    dma_display->setTextColor(dma_display->color565(255, 0, 0));
    dma_display->setCursor(5, 15);
    dma_display->print("Fluid Err");
  }
}

static void renderPixelDustView()
{
  static bool logged = false;
  if (!logged)
  {
    LOG_DEBUG_LN("PixelDust view active");
    logged = true;
  }
  PixelDustEffect();
}

static void renderFullscreenSpiralPalette()
{
  updateAndDrawFullScreenSpiral(SPIRAL_COLOR_PALETTE);
}

static void renderFullscreenSpiralWhite()
{
  updateAndDrawFullScreenSpiral(SPIRAL_COLOR_WHITE);
}

static void renderVideoPlayerView()
{
  if (videoPlayerInstance)
  {
    videoPlayerInstance->updateAndDraw(dma_display, micros());
#if DEBUG_VIDEO_PLAYER
    static unsigned long lastVideoDebugLogMs = 0;
    const MonoVideoDebugInfo info = videoPlayerInstance->debugInfo();
    const unsigned long displayedFrame = (info.frameCount > 0U) ? (info.currentFrameIndex + 1U) : 0U;
    const unsigned long nowMs = millis();
    if ((nowMs - lastVideoDebugLogMs) >= 1000UL)
    {
      Serial.printf(
          "Video dbg ready=%u open=%u frame=%lu/%lu interval=%luus rew=%lu fail=%lu catch=%u max=%u decode=%luus offset=%lu err=%s\n",
          static_cast<unsigned int>(videoPlayerInstance->ready()),
          static_cast<unsigned int>(info.fileOpen),
          displayedFrame,
          static_cast<unsigned long>(info.frameCount),
          static_cast<unsigned long>(info.frameIntervalMicros),
          static_cast<unsigned long>(info.rewindCount),
          static_cast<unsigned long>(info.decodeFailureCount),
          static_cast<unsigned int>(info.lastCatchupFrames),
          static_cast<unsigned int>(info.maxCatchupFrames),
          static_cast<unsigned long>(info.lastDecodeMicros),
          static_cast<unsigned long>(info.currentFileOffset),
          videoPlayerInstance->error());
      lastVideoDebugLogMs = nowMs;
    }

    dma_display->setFont(&TomThumb);
    dma_display->setTextSize(1);
    dma_display->setTextColor(dma_display->color565(255, 255, 0));

    char line1[24];
    char line2[24];
    char line3[24];
    snprintf(line1,
             sizeof(line1),
             "fr %lu/%lu %lums",
             displayedFrame,
             static_cast<unsigned long>(info.frameCount),
             static_cast<unsigned long>((info.frameIntervalMicros + 500U) / 1000U));
    snprintf(line2,
             sizeof(line2),
             "rw %lu de %lu c%u",
             static_cast<unsigned long>(info.rewindCount),
             static_cast<unsigned long>(info.decodeFailureCount),
             static_cast<unsigned int>(info.lastCatchupFrames));
    snprintf(line3,
             sizeof(line3),
             "dc %luus of %lu",
             static_cast<unsigned long>(info.lastDecodeMicros),
             static_cast<unsigned long>(info.currentFileOffset));

    dma_display->setCursor(1, 9);
    dma_display->print(line1);
    dma_display->setCursor(1, 17);
    dma_display->print(line2);
    dma_display->setCursor(1, 25);
    dma_display->print(line3);
#endif
    return;
  }

  dma_display->setFont(&TomThumb);
  dma_display->setTextSize(1);
  dma_display->setTextColor(dma_display->color565(255, 0, 0));
  dma_display->setCursor(3, 12);
  dma_display->print("Video Err");
  dma_display->setCursor(3, 22);
  dma_display->print("Init fail");
}

static const ViewRenderFunc VIEW_RENDERERS[TOTAL_VIEWS] = {
    renderDebugSquaresView,        // VIEW_DEBUG_SQUARES
    renderLoadingBarView,          // VIEW_LOADING_BAR
    patternPlasma,                 // VIEW_PATTERN_PLASMA
    drawTransFlag,                 // VIEW_TRANS_FLAG
    drawLGBTFlag,                  // VIEW_LGBT_FLAG
    renderFaceWithPlasma,          // VIEW_NORMAL_FACE
    renderFaceWithPlasma,          // VIEW_BLUSH_FACE
    renderFaceWithPlasma,          // VIEW_SEMICIRCLE_EYES
    renderFaceWithPlasma,          // VIEW_X_EYES
    renderFaceWithPlasma,          // VIEW_SLANT_EYES
    renderSpiralEyesView,          // VIEW_SPIRAL_EYES
    renderPlasmaFaceView,          // VIEW_PLASMA_FACE
    renderFaceWithPlasma,          // VIEW_UWU_EYES
    updateStarfield,               // VIEW_STARFIELD
    renderBsodView,                // VIEW_BSOD
    updateDVDLogos,                // VIEW_DVD_LOGO
    renderFlameEffectView,         // VIEW_FLAME_EFFECT
    renderFluidEffectView,         // VIEW_FLUID_EFFECT
    renderFaceWithPlasma,          // VIEW_CIRCLE_EYES
    renderFullscreenSpiralPalette, // VIEW_FULLSCREEN_SPIRAL_PALETTE
    renderFullscreenSpiralWhite,   // VIEW_FULLSCREEN_SPIRAL_WHITE
    renderScrollingTextView,       // VIEW_SCROLLING_TEXT
    renderDinoGameView,            // VIEW_DINO_GAME
    strobeEffect,                  // VIEW_STROBE_EFFECT
    renderPixelDustView,           // VIEW_PIXEL_DUST
    staticColor,                   // VIEW_STATIC_COLOR
    patternRainbowGradient,        // VIEW_RAINBOW_GRADIENT
    patternRainbowLinearBand,      // VIEW_RAINBOW_LINEAR_BAND
    renderFaceWithPlasma,          // VIEW_ALT_FACE
    renderVideoPlayerView,         // VIEW_VIDEO_PLAYER
};

static_assert(sizeof(VIEW_RENDERERS) / sizeof(ViewRenderFunc) == TOTAL_VIEWS, "View renderer table mismatch");

static bool viewNeedsPreClear(int view)
{
  switch (view)
  {
  case VIEW_PATTERN_PLASMA:
  case VIEW_TRANS_FLAG:
  case VIEW_LGBT_FLAG:
  case VIEW_BSOD:
  case VIEW_FLAME_EFFECT:
  case VIEW_STROBE_EFFECT:
  case VIEW_STATIC_COLOR:
  case VIEW_RAINBOW_GRADIENT:
  case VIEW_RAINBOW_LINEAR_BAND:
    return false;
  default:
    return true;
  }
}

void displayCurrentView(int view)
{
  static int previousViewLocal = -1; // Track the last active view

  // If we're in sleep mode, don't display the normal view
  if (sleepModeActive)
  {
    displaySleepMode(); // This function handles its own flipDMABuffer or drawing rate
    return;
  }


  const PairingSnapshot pairingSnapshot = getPairingSnapshot();
  if (pairingSnapshot.pairing && pairingSnapshot.passkeyValid)
  {
    dma_display->clearScreen();
    drawPairingPasskeyOverlay(pairingSnapshot.passkey);
    dma_display->flipDMABuffer();
    return;
  }
  mouthOpen = micIsMouthOpen();

  if (viewNeedsPreClear(view))
  {
    dma_display->clearScreen();
  }
  if (view != previousViewLocal)
  { // Check if the view has changed
    facePlasmaDirty = true;
    micSetEnabled(viewUsesMic(view));
    if (view == VIEW_BLUSH_FACE)
    {
      // Reset fade logic when entering the blush view
      blushStateStartTime = millis();
      isBlushFadingIn = true;
      blushState = BlushState::FadeIn; // Start fade‑in state
#if DEBUG_MODE
      Serial.println("Entered blush view, resetting fade logic");
#endif
    }
    if (view == VIEW_FLUID_EFFECT)
    { // If switching to Fluid Animation view
      if (fluidEffectInstance)
      {
        fluidEffectInstance->begin(); // Reset fluid effect state
      }
    }
    if (view == VIEW_FLAME_EFFECT)
    {
      initFlameEffect(dma_display);
    }
    if (view == VIEW_PIXEL_DUST)
    {
      setupPixelDust();
    }
    if (view == VIEW_DINO_GAME)
    {
      resetDinoGame(millis());
    }
    if (view == VIEW_VIDEO_PLAYER && videoPlayerInstance)
    {
      videoPlayerInstance->begin();
    }
    previousViewLocal = view;
  }

  ViewRenderFunc renderer = (view >= 0 && view < TOTAL_VIEWS) ? VIEW_RENDERERS[view] : nullptr;
#if DEBUG_MODE
  static unsigned long lastSlowRendererLog = 0;
  const uint32_t rendererStartMicros = micros();
#endif
  if (renderer)
  {
    renderer();
  }

  if (blushState != BlushState::Inactive)
  {
    drawBlush();
  }
#if DEBUG_MODE
  const uint32_t rendererMicros = micros() - rendererStartMicros;
  const uint32_t rendererBudgetMicros = viewFrameBudgetMicros(view);
  if (rendererMicros > rendererBudgetMicros)
  {
    const unsigned long now = millis();
    if (now - lastSlowRendererLog >= 500)
    {
      Serial.printf("Slow renderer %lu us (budget %lu us, view %d)\n",
                    static_cast<unsigned long>(rendererMicros),
                    static_cast<unsigned long>(rendererBudgetMicros),
                    view);
      lastSlowRendererLog = now;
    }
  }
#endif

#if DEBUG_FPS_COUNTER

  // --- Begin FPS counter overlay ---
  static unsigned long lastFpsTime = 0;
  static int frameCount = 0;
  frameCount++;
  unsigned long currentTime = millis();
  if (currentTime - lastFpsTime >= 1000)
  {
    fps = frameCount;
    frameCount = 0;
    lastFpsTime = currentTime;
  }
  char fpsText[8];
  sprintf(fpsText, "FPS %d", fps);
  dma_display->setTextSize(1);                                     // Set text size for FPS counter
  dma_display->setFont(&TomThumb);                                 // Use default font
  dma_display->setTextColor(dma_display->color565(255, 255, 255)); // White text
  // Position the counter at a corner (adjust as needed)
  dma_display->setCursor(37, 5); // Adjust position
  dma_display->print(fpsText);
  // --- End FPS counter overlay ---

#endif

  drawBluetoothStatusIcon();
  if (pairingSnapshot.pairing && pairingSnapshot.passkeyValid)
  {
    drawPairingPasskeyOverlay(pairingSnapshot.passkey);
  }

  if (!sleepModeActive || currentView == VIEW_TRANS_FLAG || currentView == VIEW_BSOD)
  {
    dma_display->flipDMABuffer();
  }
}

// Helper function to get the current threshold value
float getCurrentThreshold()
{
  return useShakeSensitivity ? SHAKE_THRESHOLD : SLEEP_THRESHOLD;
}

// Add with your other utility functions
// constexpr unsigned long MOTION_SAMPLE_INTERVAL_FAST = 15;  // ms between accel reads when looking for shakes
constexpr unsigned long MOTION_SAMPLE_INTERVAL_SLOW = 120; // ms between reads for sleep detection

static unsigned long lastAccelSampleTime = 0;

static bool refreshAccelSampleIfStale(const unsigned long sampleInterval)
{
  if (!accelerometerEnabled || !g_accelerometer_initialized)
  {
    return false;
  }

  const unsigned long now = millis();
  if (!accelSampleValid || (now - lastAccelSampleTime) >= sampleInterval)
  {
    PROFILE_SECTION("AccelRead");
    accel.read();

    const float x = accel.x_g;
    const float y = accel.y_g;
    const float z = accel.z_g;

    // Calculate the movement delta from last reading
    lastAccelDeltaX = fabs(x - prevAccelX);
    lastAccelDeltaY = fabs(y - prevAccelY);
    lastAccelDeltaZ = fabs(z - prevAccelZ);

    // Store current values for next comparison and reuse across effects
    prevAccelX = x;
    prevAccelY = y;
    prevAccelZ = z;
    lastAccelSampleTime = now;
    accelSampleValid = true;

    // Update cached comparisons for both thresholds so other checks can reuse this sample
    lastMotionAboveShake = (lastAccelDeltaX > SHAKE_THRESHOLD) || (lastAccelDeltaY > SHAKE_THRESHOLD) || (lastAccelDeltaZ > SHAKE_THRESHOLD);
    lastMotionAboveSleep = (lastAccelDeltaX > SLEEP_THRESHOLD) || (lastAccelDeltaY > SLEEP_THRESHOLD) || (lastAccelDeltaZ > SLEEP_THRESHOLD);
  }

  return accelSampleValid;
}

bool getLatestAcceleration(float &x, float &y, float &z, const unsigned long sampleInterval)
{
  if (!refreshAccelSampleIfStale(sampleInterval))
  {
    return false;
  }

  x = prevAccelX;
  y = prevAccelY;
  z = prevAccelZ;
  return true;
}

bool detectMotion()
{
  if (!accelerometerEnabled || !g_accelerometer_initialized)
  {
    return false; // Guard against uninitialised accelerometer
  }

  const unsigned long sampleInterval = useShakeSensitivity ? MOTION_SAMPLE_INTERVAL_FAST : MOTION_SAMPLE_INTERVAL_SLOW;
  refreshAccelSampleIfStale(sampleInterval);

  if (currentView == VIEW_PIXEL_DUST)
  {
    useShakeSensitivity = false; // Disable shake-to-spiral while sand is active.
    return false;
  }

  // Return the cached comparison for the desired threshold without re-reading.
  return useShakeSensitivity ? lastMotionAboveShake : lastMotionAboveSleep;
}

void enterSleepMode()
{
  if (sleepModeActive)
    return; // Already sleeping
  #if DEBUG_MODE
  Serial.println("Entering sleep mode");
  #endif
  sleepModeActive = true;
  micSetEnabled(false);
  preSleepView = currentView;                   // Save current view
  dma_display->setBrightness8(sleepBrightness); // Lower display brightness
  updateGlobalBrightnessScale(sleepBrightness);
  requestDisplayRefresh();

  reduceCPUSpeed(); // Reduce CPU speed for power saving

  sleepFrameInterval = 100; // 10 FPS during sleep to save power

  /// Reduce BLE activity
  if (!deviceConnected)
  {
    NimBLEDevice::getAdvertising()->stop();               // Stop normal advertising
    vTaskDelay(pdMS_TO_TICKS(10));                        // Short delay
    NimBLEDevice::getAdvertising()->setMinInterval(2400); // 1500 ms
    NimBLEDevice::getAdvertising()->setMaxInterval(4800); // 3000 ms
    refreshBleAdvertising();
    #if DEBUG_MODE
    Serial.println("Reduced BLE Adv interval for sleep.");
    #endif
  }
  else
  {
    // Maybe send a sleep notification?
    if (pTemperatureCharacteristic != nullptr)
    { // Check if characteristic exists
      char sleepMsg[] = "Sleep";
      pTemperatureCharacteristic->setValue(sleepMsg);
      notifyBleTask();
      // pTemperatureCharacteristic->notify(); // Consider notifyPending
    }
  }
  // No need to change sleepFrameInterval here, main loop timing controls frame rate
}

void checkSleepMode()
{
  const unsigned long now = millis();
  // Ensure we use the correct sensitivity for wake-up/sleep checks
  useShakeSensitivity = false; // Use SLEEP_THRESHOLD when checking general activity/wake conditions
  bool motionDetectedByAccel = false;
  if (accelerometerEnabled && g_accelerometer_initialized)
  { // Only check accel if enabled and initialized
    const bool sampleFresh = accelSampleValid && (now - lastAccelSampleTime) < MOTION_SAMPLE_INTERVAL_SLOW;
    if (!sampleFresh)
    {
      detectMotion(); // Updates cached deltas for both thresholds
    }
    motionDetectedByAccel = lastMotionAboveSleep;
  }

  if (sleepModeActive)
  {
    // --- Currently Sleeping ---
    if (motionDetectedByAccel)
    {
      #if DEBUG_MODE
      Serial.println("Motion detected while sleeping, waking up..."); // DEBUG
      #endif
      wakeFromSleepMode();                                            // Call the wake function
                                                                      // wakeFromSleepMode already sets sleepModeActive = false and resets lastActivityTime
    }
    // No need to check timeout when already sleeping
  }
  else
  {
    // --- Currently Awake ---
    if (motionDetectedByAccel)
    {
      // Any motion resets the activity timer when awake
      lastActivityTime = now;
      // Serial.println("Motion detected while awake, activity timer reset."); // DEBUG Optional
    }
    else
    {
      // No motion detected while awake, check timeout for sleep entry
      // Ensure sleepModeEnabled config flag is checked
      const unsigned long inactivityMs = (now >= lastActivityTime) ? (now - lastActivityTime) : 0;
      if (sleepModeEnabled && (inactivityMs > SLEEP_TIMEOUT_MS))
      {
        #if DEBUG_MODE
        Serial.println("Inactivity timeout reached, entering sleep..."); // DEBUG
        #endif
        enterSleepMode();
      }
    }
  }
}

void loop()
{
  const uint32_t loopStartMicros = micros();

  // Serial.println(apds.readProximity());

  // unsigned long frameStartTimeMillis = millis(); // Timestamp at frame start
  const unsigned long loopNow = millis();

  if (expirePairingModeIfNeeded())
  {
    refreshBleAdvertising();
    requestDisplayRefresh();
  }

  flushPendingLastViewPersist(loopNow);

  // --- Handle Inputs and State Updates ---

  // Check for motion and handle sleep/wake state
  // checkSleepMode(); // Handles motion detection, wake, and sleep entry

  // Only process inputs/updates if NOT in sleep mode
  if (!sleepModeActive)
  {
    // Update animation state first so the display task can render with the freshest values.
    runIfElapsed(loopNow, lastAnimationUpdateTime, ANIMATION_UPDATE_INTERVAL_MS, [&]()
                 {
                   PERF_SCOPE(PerfBucket::Animation);
                   PROFILE_SECTION("AnimationUpdates");
                   updateBlinkAnimation();     // Update blink animation once per interval
                   updateEyeBounceAnimation(); // Update eye bounce animation progress
                   updateIdleHoverAnimation(); // Update idle eye hover animation progress
                 });

    if (blushState != BlushState::Inactive)
    {
      PROFILE_SECTION("BlushUpdate");
      updateBlush();
    }

    // --- Motion Detection (for shake effect to change view) ---
    runIfElapsed(loopNow, lastMotionCheckTime, MOTION_CHECK_INTERVAL_MS, [&]()
                 {
                   PERF_SCOPE(PerfBucket::Sensors);
                   PROFILE_SECTION("MotionDetection");
                   #if defined(BUTTON_UP) && defined(BUTTON_DOWN)
                   const bool buttonPhysicallyPressed = (digitalRead(BUTTON_UP) == LOW) || (digitalRead(BUTTON_DOWN) == LOW);
                   #else
                   const bool buttonPhysicallyPressed = false;
                   #endif

                   if (!buttonPhysicallyPressed &&
                       !shakeSuppressedByRecentButton(loopNow) &&
                       accelerometerEnabled &&
                       g_accelerometer_initialized &&
                       currentView != VIEW_FLUID_EFFECT &&
                       currentView != VIEW_DINO_GAME &&
                       currentView != VIEW_RAINBOW_GRADIENT &&
                       currentView != VIEW_RAINBOW_LINEAR_BAND &&
                       !isEyeBouncing &&
                       !proximityLatchedHigh)
                   {
                     useShakeSensitivity = true; // Use high threshold for shake detection
                     if (detectMotion())
                     { // detectMotion uses the current useShakeSensitivity
                       if (currentView != VIEW_SPIRAL_EYES)
                       {                                 // Prevent re-triggering if already spiral
                         previousView = currentView;     // Save the current view.
                         currentView = VIEW_SPIRAL_EYES; // Switch to spiral eyes view
                         spiralStartMillis = loopNow;    // Record the trigger time.
                         requestDisplayRefresh();
                         LOG_DEBUG_LN("Shake detected! Switching to Spiral View.");
                         notifyBleTask();
                         lastActivityTime = loopNow; // Shake is activity
                       }
                     }
                     useShakeSensitivity = false; // Switch back to low threshold for general sleep/wake checks
                   }
                 });

// --- Handle button inputs for view changes ---
#if defined(BUTTON_UP) && defined(BUTTON_DOWN)
    PERF_SCOPE(PerfBucket::Io);
    static ButtonState upButtonState;
    static ButtonState downButtonState;
    static bool pairingHoldActive = false;
    static bool pairingHoldTriggered = false;
    static unsigned long pairingHoldStartMs = 0;

    updateButtonState(upButtonState, digitalRead(BUTTON_UP) == LOW, loopNow, BUTTON_DEBOUNCE_MS);
    updateButtonState(downButtonState, digitalRead(BUTTON_DOWN) == LOW, loopNow, BUTTON_DEBOUNCE_MS);

    const bool upPressed = upButtonState.stablePressed;
    const bool downPressed = downButtonState.stablePressed;
    const bool pairingHoldPressed = upPressed && downPressed;

    if (pairingHoldPressed)
    {
      suppressButtonPress(upButtonState);
      suppressButtonPress(downButtonState);
      noteButtonInteraction(loopNow);
      lastActivityTime = loopNow;
      if (!pairingHoldActive)
      {
        pairingHoldActive = true;
        pairingHoldStartMs = loopNow;
      }
      else if (!pairingHoldTriggered && (loopNow - pairingHoldStartMs >= PAIRING_RESET_HOLD_MS))
      {
        pairingHoldTriggered = true;
        resetBlePairing();
      }
    }
    else
    {
      pairingHoldActive = false;
      pairingHoldTriggered = false;
    }

    bool viewChangedByButton = false;
    bool pairingModeTriggered = false;

    if (!pairingHoldPressed && upPressed && !downPressed &&
        consumeButtonLongPress(upButtonState, loopNow, PAIRING_MODE_HOLD_MS))
    {
      noteButtonInteraction(loopNow);
      startPairingMode(PAIRING_MODE_WINDOW_MS);
      refreshBleAdvertising();
      lastActivityTime = loopNow;
      pairingModeTriggered = true;
      requestDisplayRefresh();
    }

    if (!pairingHoldPressed && downPressed && !upPressed &&
        consumeButtonLongPress(downButtonState, loopNow, PAIRING_MODE_HOLD_MS))
    {
      noteButtonInteraction(loopNow);
      startPairingMode(PAIRING_MODE_WINDOW_MS);
      refreshBleAdvertising();
      lastActivityTime = loopNow;
      pairingModeTriggered = true;
      requestDisplayRefresh();
    }

    if (!pairingHoldPressed && consumeButtonShortPress(upButtonState))
    {
      noteButtonInteraction(loopNow);
      currentView = (currentView + 1);
      if (currentView >= totalViews)
        currentView = 0;
      viewChangedByButton = true;
      scheduleLastViewPersist(currentView);
      lastActivityTime = loopNow;
      requestDisplayRefresh();
    }

    if (!pairingHoldPressed && consumeButtonShortPress(downButtonState))
    {
      PROFILE_SECTION("ButtonInputs");
      noteButtonInteraction(loopNow);
      currentView = (currentView - 1);
      if (currentView < 0)
      {
        currentView = totalViews - 1;
      }
      viewChangedByButton = true;
      scheduleLastViewPersist(currentView);
      lastActivityTime = loopNow;
      requestDisplayRefresh();
    }

    if (pairingModeTriggered)
    {
      LOG_DEBUG_LN("BLE pairing mode enabled by long button press.");
    }

    if (viewChangedByButton)
    {
      LOG_DEBUG("View changed by button: %d\n", currentView);
      notifyBleTask();
      // Reset specific view states if necessary when changing views
      if (currentView != VIEW_BLUSH_FACE)
      { // If leaving blush view
        blushState = BlushState::Inactive;
        blushBrightness = 0;
        // disableBlush(); // Clears pixels, but displayCurrentView will clear anyway
      }
      if (currentView != VIEW_SPIRAL_EYES)
      { // Reset spiral timer
        spiralStartMillis = 0;
      }
    }
#endif

    // --- Proximity Sensor Logic: Blush Trigger AND Eye Bounce Trigger ---
    // static unsigned long lastSensorReadTime = 0; // Already global
    const bool deferProximityRead = shouldDeferProximityRead(loopNow, PROX_LUX_READ_GUARD_MS);
    if (!deferProximityRead)
    {
      runIfElapsed(loopNow, lastSensorReadTime, sensorInterval, [&]()
                   {
                    PERF_SCOPE(PerfBucket::Sensors);
#if DEBUG_MODE
                   PROFILE_SECTION("ProximitySensor");
#endif
#if defined(APDS_AVAILABLE) // Ensure sensor is available
                   const unsigned long sensorNow = loopNow;
                   if (currentView == VIEW_RAINBOW_GRADIENT || currentView == VIEW_RAINBOW_LINEAR_BAND)
                   {
                     proximityLatchedHigh = false;
                     return;
                   }

                   uint8_t proximity = 0;
                   uint32_t proxDuration = 0;
                   if (!readAmbientProximity(sensorNow, proximity, &proxDuration))
                   {
#if DEBUG_PROXIMITY
                     LOG_PROX("Prox skip @%lu ms\n", sensorNow);
#endif
                     return;
                   }

#if DEBUG_MODE
                   if (proxDuration > SLOW_SECTION_THRESHOLD_US)
                   {
                     LOG_DEBUG("Slow proximity read: %lu us\n", static_cast<unsigned long>(proxDuration));
                   }
#endif
#if DEBUG_PROXIMITY
                   LOG_PROX("Prox=%u dt=%lu us\n", proximity, proxDuration);
#endif

                   bool bounceJustTriggered = false; // Flag to avoid double blush trigger

                   if (!proximityBaselineValid)
                   {
                     proximityBaseline = static_cast<float>(proximity);
                     proximityBaselineValid = true;
                   }

                   int baselineRounded = static_cast<int>(proximityBaseline + 0.5f);
                   uint8_t triggerThreshold = static_cast<uint8_t>(
                       constrain(baselineRounded + PROX_TRIGGER_DELTA, 0, 255));
                   uint8_t releaseThreshold = static_cast<uint8_t>(
                       constrain(baselineRounded + PROX_RELEASE_DELTA, 0, 255));

                   if (!proximityLatchedHigh && proximity <= releaseThreshold)
                   {
                     proximityBaseline += PROX_BASELINE_ALPHA * (static_cast<float>(proximity) - proximityBaseline);
                     baselineRounded = static_cast<int>(proximityBaseline + 0.5f);
                     triggerThreshold = static_cast<uint8_t>(
                         constrain(baselineRounded + PROX_TRIGGER_DELTA, 0, 255));
                     releaseThreshold = static_cast<uint8_t>(
                         constrain(baselineRounded + PROX_RELEASE_DELTA, 0, 255));
                   }

                   // Require the reading to rise above the trigger threshold and then fall below the release threshold before re-triggering
                   if (proximityLatchedHigh)
                   {
                     const bool timeout = (sensorNow - proximityLatchedAt) >= PROX_LATCH_TIMEOUT_MS;
                     if (proximity <= releaseThreshold || (timeout && proximity < triggerThreshold))
                     {
                       proximityLatchedHigh = false;
#if DEBUG_PROXIMITY
                       LOG_PROX("Prox released at %u (rel=%u base=%d)\n", proximity, releaseThreshold, baselineRounded);
#endif
                     }
                     else
                     {
#if DEBUG_PROXIMITY
                       LOG_PROX("Prox latched high (%u), skipping trigger\n", proximity);
#endif
                       return;
                     }
                   }

                   if (proximity < triggerThreshold)
                   {
                     return;
                   }

                   proximityLatchedHigh = true;
                   proximityLatchedAt = sensorNow;

                   if (currentView == VIEW_DINO_GAME)
                   {
                     queueDinoJump();
                     lastActivityTime = sensorNow;
                     return;
                   }

                   // Eye Bounce Trigger - Switch to View 17 (Circle Eyes) (MODIFIED)
                   if (!isEyeBouncing)
                   {
                     LOG_DEBUG_LN("Proximity! Starting eye bounce sequence & switching to Circle Eyes (View 17).");
#if DEBUG_PROXIMITY
                     LOG_PROX("Bounce trigger prox=%u view=%d\n", proximity, currentView);
#endif

                     // Store current view and switch to Circle Eyes (view 17)
                     if (currentView != VIEW_CIRCLE_EYES)
                     {
                       viewBeforeEyeBounce = currentView;
                       currentView = VIEW_CIRCLE_EYES; // Switch to "Circle Eyes" view
                       requestDisplayRefresh();
                       // saveLastView(currentView); // Optional: save temporary view 17
                       notifyBleTask();
                     }

                     isEyeBouncing = true;
                     eyeBounceStartTime = sensorNow;
                     eyeBounceCount = 0;
                     lastActivityTime = sensorNow;
                     bounceJustTriggered = true; // Mark that bounce (and view switch) happened

                     // Also trigger blush effect to happen ON view 17 (Circle Eyes)
                     if (blushState == BlushState::Inactive)
                     { // Only trigger if not already blushing
                       LOG_DEBUG_LN("Proximity! Also triggering blush effect on Circle Eyes.");
#if DEBUG_PROXIMITY
                       LOG_PROX("Blush-on-bounce prox=%u\n", proximity);
#endif
                       blushState = BlushState::FadeIn;
                       blushStateStartTime = sensorNow;
                       wasBlushOverlay = false;                       // Blush on view 17 is part of that view's temporary effect
                       originalViewBeforeBlush = viewBeforeEyeBounce; // Blush on view 17 uses context of viewBeforeEyeBounce
                     }
                   }

                   if (blushState == BlushState::Inactive && !bounceJustTriggered)
                   {
                     // Trigger blush if:
                     // 1. Proximity detected
                     // 2. Not already blushing
                     // 3. Bounce wasn't *just* triggered (which might have handled its own blush)
                     // 4. Current view is NOT the dedicated blush view (5)
                     //    AND current view is NOT the temporary bounce/circle view (17)
                     if (currentView != VIEW_BLUSH_FACE && currentView != VIEW_CIRCLE_EYES)
                     {
                       LOG_DEBUG_LN("Proximity! Triggering blush overlay effect.");
#if DEBUG_PROXIMITY
                       LOG_PROX("Blush overlay prox=%u view=%d\n", proximity, currentView);
#endif
                       blushState = BlushState::FadeIn;
                       blushStateStartTime = sensorNow;
                       lastActivityTime = sensorNow;

                       // This is a blush overlay on the current stable view
                       originalViewBeforeBlush = currentView; // Store the view that is getting the overlay
                       wasBlushOverlay = true;                // Mark that this blush is an overlay
                     }
                   }
#endif
                   });
    }

    // --- Revert from Spiral View Timer ---
    // Use a local copy to avoid updating spiralStartMillis inside hasElapsedSince
    unsigned long spiralStartMillisCopy = spiralStartMillis;
    if (currentView == VIEW_SPIRAL_EYES && spiralStartMillisCopy > 0 && hasElapsedSince(loopNow, spiralStartMillisCopy, 5000))
    {
      LOG_DEBUG_LN("Spiral timeout, reverting view.");
      currentView = previousView;
      spiralStartMillis = 0;
      requestDisplayRefresh();
      notifyBleTask();
    }

    // --- Update Adaptive Brightness ---
    // Brightness smoothing runs continuously; lux sampling cadence is enforced inside updateAdaptiveBrightness().
    runIfElapsed(loopNow, lastBrightnessUpdateTime, BRIGHTNESS_UPDATE_INTERVAL_MS, [&]()
                 {
                   PERF_SCOPE(PerfBucket::Sensors);
                   PROFILE_SECTION("AutoBrightness");
                   maybeUpdateBrightness();
                 });
    if (deviceConnected)
    {
      runIfElapsed(loopNow, lastLuxUpdateTime, LUX_UPDATE_INTERVAL_MS, [&]()
                   {
                     PERF_SCOPE(PerfBucket::Ble);
                     PROFILE_SECTION("LuxCharacteristic");
                     // Manual brightness is applied immediately on BLE write if autoBrightness is off.
                     updateLux(); // Update lux values
                   });
    }

    // --- Update Temperature Sensor Periodically ---
    static unsigned long lastTempUpdateLocal = 0;       // Renamed to avoid conflict
    const unsigned long tempUpdateIntervalLocal = 5000; // 5 seconds
    runIfElapsed(loopNow, lastTempUpdateLocal, tempUpdateIntervalLocal, [&]()
                 {
                   PERF_SCOPE(PerfBucket::Sensors);
#if DEBUG_MODE
                   PROFILE_SECTION("TemperatureUpdate");
#endif
                   maybeUpdateTemperature(); });

  } // End of (!sleepModeActive) block

  // --- Display Rendering ---
  {
    PROFILE_SECTION("MouthMovement");
  }
  // displayCurrentView(currentView); // Draw the appropriate view based on state

  // --- Check sleep again AFTER processing inputs ---
  // This allows inputs like button presses or BLE commands to reset the activity timer *before* checking for sleep timeout
  runIfElapsed(loopNow, lastSleepCheckTime, SLEEP_CHECK_INTERVAL_MS, [&]()
               {
                 PERF_SCOPE(PerfBucket::Sensors);
                 PROFILE_SECTION("SleepModeCheck");
                 checkSleepMode(); });

  // Check BLE connection status (low frequency check is fine)
  // bool isConnected = NimBLEDevice::getServer()->getConnectedCount() > 0;
  runIfElapsed(loopNow, lastBleStatusLedUpdateTime, BLE_STATUS_LED_INTERVAL_MS, [&]()
               {
                 PERF_SCOPE(PerfBucket::Io);
                 PROFILE_SECTION("BLEStatusLED");
                 handleBLEStatusLED(); // Update status LED based on connection
               });

  // Check for changes in brightness
  // checkBrightness();

  // --- Frame Rate Calculation ---
  {
    PROFILE_SECTION("FPSCalc");
    calculateFPS(); // Update FPS counter
  }

  perfRecordLoopDuration(micros() - loopStartMicros);
  perfMaybeReport(loopNow);

  const TickType_t idleDelayTicks = sleepModeActive ? pdMS_TO_TICKS(5) : pdMS_TO_TICKS(1);
  vTaskDelay(idleDelayTicks);

  // vTaskDelay(pdMS_TO_TICKS(5)); // yield to the display & BLE tasks

  /*
  // If the current view is one of the plasma views, increase the interval to reduce load
  if (currentView == VIEW_PATTERN_PLASMA || currentView == VIEW_PLASMA_FACE) {
    baseInterval = 10; // Use 15 ms for plasma view
  }
  */
}
