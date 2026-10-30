#include "rendering/RenderSupport.h"
#include "rendering/FaceRenderer.h"
#include "rendering/EffectRenderer.h"
#include "rendering/ScrollRenderer.h"
#include "rendering/DisplayOverlays.h"
#include "app/ViewManager.h"
#include "app/RuntimeTuning.h"
#include "core/DisplayTiming.h"

namespace {
TaskHandle_t displayTaskHandle = nullptr;
constexpr unsigned long PATTERN_PLASMA_FRAME_INTERVAL_MS = LF_FACE_VIEW_FRAME_INTERVAL_MS;
constexpr uint8_t VIEW_TRANSITION_FADE_STEPS = 8;
uint16_t fps = 0;
#if DEBUG_MODE
uint32_t gLastFrameDurationMicros = 0;
uint32_t gWorstFrameDurationMicros = 0;
#endif
}
using ViewRenderFunc = void (*)();
constexpr TickType_t millisToTicksCeil(unsigned long milliseconds)
{
  return static_cast<TickType_t>((milliseconds + portTICK_PERIOD_MS - 1UL) / portTICK_PERIOD_MS);
}

bool viewUsesMic(int view)
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
  case VIEW_MATRIX_FACE:   // ADD — mouth mic on face rain
    return true;
  default:
    return false;
  }
}

bool viewNeedsContinuousRefresh(int view)
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

unsigned long viewFrameIntervalMillis(int view)
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
  case VIEW_MATRIX_FACE:   // ADD
    return PATTERN_PLASMA_FRAME_INTERVAL_MS;
  case VIEW_DVD_LOGO:
    return runtime::kDvdFrameIntervalMs;
  default:
    return runtime::kBaseFrameIntervalMs;
  }
}

uint32_t viewFrameBudgetMicros(int view)
{
  unsigned long frameIntervalMs = viewFrameIntervalMillis(view);
  if (bleIsOtaBusy() && frameIntervalMs < LF_OTA_ACTIVE_FRAME_INTERVAL_MS)
  {
    frameIntervalMs = LF_OTA_ACTIVE_FRAME_INTERVAL_MS;
  }
  return static_cast<uint32_t>(frameIntervalMs) * 1000UL;
}

void displayTask(void *pvParameters)
{
  (void)pvParameters;
  uint32_t lastWake = xTaskGetTickCount();
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
    AppStateLock stateLock;
    // Consume once between frames, never while another task is drawing pixels.
    // Coalescing prevents stale fade steps from building up if rendering is busy.
    applyPendingDisplayBrightness();
    const int requestedView = static_cast<int>(currentView);

    if (sleepModeActive)
    {
      transitionPhase = ViewTransitionPhase::Idle;
      transitionLevel = VIEW_TRANSITION_FADE_STEPS;
      displayedView = requestedView;
      pendingView = requestedView;
      displayCurrentView(requestedView);
      perfTelemetryRecordDuration(PerfDurationId::DisplayTaskBusy, micros() - taskLoopStartMicros);
      stateLock.unlock();
      vTaskDelay(display_timing::nextFrameDelay(xTaskGetTickCount(), lastWake, millisToTicksCeil(sleepFrameInterval)));
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
      stateLock.unlock();
      ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
      lastWake = xTaskGetTickCount();
      continue;
    }

    if (bleIsOtaBusy() && frameIntervalMs < LF_OTA_ACTIVE_FRAME_INTERVAL_MS)
    {
      frameIntervalMs = LF_OTA_ACTIVE_FRAME_INTERVAL_MS;
    }
    perfTelemetryRecordDuration(PerfDurationId::DisplayTaskBusy, micros() - taskLoopStartMicros);
    stateLock.unlock();
    vTaskDelay(display_timing::nextFrameDelay(xTaskGetTickCount(), lastWake, millisToTicksCeil(frameIntervalMs)));
  }
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
    renderMatrixRainView,          // VIEW_MATRIX_RAIN
    renderMatrixFaceView,          // VIEW_MATRIX_FACE
    renderFaceWithPlasma,          // VIEW_ANGRY_FACE
};

static_assert(sizeof(VIEW_RENDERERS) / sizeof(ViewRenderFunc) == TOTAL_VIEWS, "View renderer table mismatch");


bool viewNeedsPreClear(int view)
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
  case VIEW_MATRIX_RAIN:   // ADD — effect clears the screen itself
    return false;
  default:
    return true;
  }
}

void displayCurrentView(int view)
{
  matrixRainBeginFrame();
  static int previousViewLocal = -1; // Track the last active view

  // If we're in sleep mode, don't display the normal view
  if (sleepModeActive)
  {
    applyMouthMicBrightnessPolicy(false);
    displaySleepMode(); // This function handles its own flipDMABuffer or drawing rate
    return;
  }

  const PairingSnapshot pairingSnapshot = getPairingSnapshot();
  if (pairingSnapshot.pairing && pairingSnapshot.passkeyValid)
  {
    applyMouthMicBrightnessPolicy(false);
    dma_display->clearScreen();
    drawPairingPasskeyOverlay(pairingSnapshot.passkey);
    presentDisplayFrame();
    return;
  }
  applyMouthMicBrightnessPolicy(viewUsesMic(view));

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
    resetEffectView(view);
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
  const uint8_t fpsBrightness = scaleRawFaceComponentForPanelHeadroom(255);
  dma_display->setTextColor(dma_display->color565(
      fpsBrightness, fpsBrightness, fpsBrightness)); // White text
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
    presentDisplayFrame();
  }
}

void requestDisplayRefresh()
{
  if (displayTaskHandle) xTaskNotifyGive(displayTaskHandle);
}

bool displayOwnsCurrentTask()
{
  return !displayTaskHandle || xTaskGetCurrentTaskHandle() == displayTaskHandle;
}

uint32_t displayTaskStackHighWaterMark()
{
  return displayTaskHandle ? uxTaskGetStackHighWaterMark(displayTaskHandle) : 0;
}

bool startViewManager()
{
  return xTaskCreatePinnedToCore(displayTask, "Display Task", 16384, nullptr, 3,
                               &displayTaskHandle, 0) == pdPASS;
}
