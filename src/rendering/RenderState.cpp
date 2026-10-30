#include "rendering/DisplaySurface.h"
#include "rendering/RenderState.h"
#include "app/ViewManager.h"
#include "app/AppRuntime.h"
#include "config/RuntimeSettings.h"
#include "config/debug_config.h"
#include "core/mic/mic_math.h"
#include <atomic>

CRGBPalette16 currentPalette = ForestColors_p;
uint16_t time_counter = 0;
uint16_t plasmaFrameDelay = 15;
unsigned long rotationFrameInterval = 15;
int loadingProgress = 0;
int loadingMax = 100;

float globalBrightnessScale = 0.0f;
uint16_t globalBrightnessScaleFixed = 256;
static volatile uint8_t gRequestedPanelBrightness = 255;
static uint8_t gRequestedHardwareBrightness = 255;
// Other tasks publish only the latest request; the renderer owns DMA writes.
static std::atomic<int> gPendingPanelBrightness{-1};
// Mouth boost raises the panel ceiling and compensates non-mouth pixels together.
static volatile bool gMouthMicBrightnessOverrideActive = false;
bool facePlasmaDirty = true;
void updateGlobalBrightnessScale(uint8_t brightness)
{
  if (!displayOwnsCurrentTask())
  {
    gPendingPanelBrightness.store(brightness, std::memory_order_relaxed);
    requestDisplayRefresh();
    return;
  }

  gRequestedPanelBrightness = brightness;
  globalBrightnessScale = brightness / 255.0f;
  globalBrightnessScaleFixed = static_cast<uint16_t>((static_cast<uint32_t>(brightness) * 256u + 127u) / 255u);
  // Boost changes software gain; scan timing stays on the stable configuration.
  if (waveshareBrightnessBoost && globalBrightnessScaleFixed < 256)
  {
    uint16_t boosted = globalBrightnessScaleFixed + (globalBrightnessScaleFixed >> 2);
    if (boosted > 256)
    {
      boosted = 256;
    }
    globalBrightnessScaleFixed = boosted;
  }
  // Apply OE only to the completed back buffer immediately before presenting.
  gRequestedHardwareBrightness = micResolvePanelBrightness(
      brightness, gMouthMicBrightnessOverrideActive);
  facePlasmaDirty = true;
  requestDisplayRefresh();
}

void presentDisplayFrame()
{
#ifdef VIRTUAL_PANE
  MatrixPanel_I2S_DMA *output = chain;
#else
  MatrixPanel_I2S_DMA *output = dma_display;
#endif
  if (output != nullptr)
  {
    output->setBackBufferBrightness(gRequestedHardwareBrightness);
    output->flipDMABuffer();
  }
}

uint16_t getNormalFaceSoftwareBrightnessScale()
{
  return micResolveFaceBrightnessScale(
      gRequestedPanelBrightness, gMouthMicBrightnessOverrideActive);
}

uint8_t scaleRawFaceComponentForPanelHeadroom(uint8_t value)
{
  if (!gMouthMicBrightnessOverrideActive)
  {
    return value;
  }
  return static_cast<uint8_t>(
      (static_cast<uint16_t>(value) * globalBrightnessScaleFixed + 128u) >> 8);
}

void applyMouthMicBrightnessPolicy(bool allowForCurrentView)
{
  const bool overrideActive = micShouldApplyPanelHeadroom(
      mouthMicBrightnessOverrideEnabled,
      allowForCurrentView);
  if (overrideActive == gMouthMicBrightnessOverrideActive)
  {
    return;
  }

  // Keep the override curve active for the entire microphone face. At idle it
  // starts at the current user/auto brightness floor, then rises to 255 with
  // microphone activity.
  gMouthMicBrightnessOverrideActive = overrideActive;
  updateGlobalBrightnessScale(gRequestedPanelBrightness);
  facePlasmaDirty = true;
  requestDisplayRefresh();
#if DEBUG_BRIGHTNESS
  if (overrideActive)
  {
    Serial.println("Mouth microphone brightness override active: panel ceiling at 255, mouth pixels bypass normal scaling.");
  }
  else
  {
    Serial.printf("Mouth microphone brightness override ended: restored panel output to %u.\n",
                  gRequestedPanelBrightness);
  }
#endif
}

void applyPendingDisplayBrightness()
{
  const int pending = gPendingPanelBrightness.exchange(-1, std::memory_order_relaxed);
  if (pending >= 0) updateGlobalBrightnessScale(static_cast<uint8_t>(pending));
}

bool mouthMicBrightnessOverrideActive() { return gMouthMicBrightnessOverrideActive; }
uint8_t requestedPanelBrightness() { return gRequestedPanelBrightness; }
