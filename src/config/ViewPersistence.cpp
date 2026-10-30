#include "config/ViewPersistence.h"
#include "config/userPreferences.h"
#include "app/AppRuntime.h"
#include "app/Timing.h"
#include "perf_tuning.h"
#include "freertos/FreeRTOS.h"

static portMUX_TYPE gLastViewPersistMux = portMUX_INITIALIZER_UNLOCKED;
static volatile bool gLastViewPersistDirty = false;
static volatile uint8_t gPendingLastView = 0;
static volatile unsigned long gPendingLastViewChangedMs = 0;
static uint8_t gLastPersistedView = 0;

void scheduleLastViewPersist(uint8_t view)
{
  const unsigned long nowMs = millis();
  portENTER_CRITICAL(&gLastViewPersistMux);
  gPendingLastView = view;
  gPendingLastViewChangedMs = nowMs;
  gLastViewPersistDirty = true;
  portEXIT_CRITICAL(&gLastViewPersistMux);
}

void flushPendingLastViewPersist(unsigned long nowMs)
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

void initializeViewPersistence(uint8_t view) { gLastPersistedView = view; }
