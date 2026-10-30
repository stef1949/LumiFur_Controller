"""Run the production view dispatcher against recorded renderer calls."""
from pathlib import Path
import re
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


def function(source, name):
    match = re.search(r"^(?:void|bool|unsigned long|uint32_t) " + re.escape(name) + r"\(", source, re.M)
    start = match.start()
    brace = source.index("{", start)
    depth, end = 1, brace + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


STUB = r'''
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cassert>
#include "app/Views.h"
#include "app/RuntimeTuning.h"
#include "core/AnimationState.h"
#include "perf_tuning.h"
#define DEBUG_MODE 0
#define DEBUG_FPS_COUNTER 0
using animation::BlushState;
struct Display {unsigned clears=0;void clearScreen(){++clears;}} display;
auto dma_display=&display;
uint32_t clockMs=100;
uint32_t millis(){return clockMs;}
bool sleepModeActive=false,facePlasmaDirty=false,micEnabled=false,headroom=false,otaActive=false;
uint8_t currentView=0;
unsigned long blushStateStartTime=0;
bool isBlushFadingIn=false;
BlushState blushState=BlushState::Inactive;
unsigned presents=0,overlays=0,blushes=0,resets=0;
int resetView=-1;
const char* renderer=nullptr;
constexpr unsigned long PATTERN_PLASMA_FRAME_INTERVAL_MS=LF_FACE_VIEW_FRAME_INTERVAL_MS;
struct PairingSnapshot {bool pairing=false,passkeyValid=false;uint32_t passkey=0;} pairing;
PairingSnapshot getPairingSnapshot(){return pairing;}
void matrixRainBeginFrame(){}
void applyMouthMicBrightnessPolicy(bool enabled){headroom=enabled;}
void micSetEnabled(bool enabled){micEnabled=enabled;}
void presentDisplayFrame(){++presents;}
void drawPairingPasskeyOverlay(uint32_t key){assert(key==123456);++overlays;}
void drawBluetoothStatusIcon(){}
void drawBlush(){++blushes;}
void resetEffectView(int view){resetView=view;++resets;}
bool bleIsOtaBusy(){return otaActive;}
#define RENDERER(name) void name(){renderer=#name;}
RENDERER(renderDebugSquaresView) RENDERER(renderLoadingBarView) RENDERER(patternPlasma)
RENDERER(drawTransFlag) RENDERER(drawLGBTFlag) RENDERER(renderFaceWithPlasma)
RENDERER(renderSpiralEyesView) RENDERER(renderPlasmaFaceView) RENDERER(updateStarfield)
RENDERER(renderBsodView) RENDERER(updateDVDLogos) RENDERER(renderFlameEffectView)
RENDERER(renderFluidEffectView) RENDERER(renderFullscreenSpiralPalette)
RENDERER(renderFullscreenSpiralWhite) RENDERER(renderScrollingTextView)
RENDERER(renderDinoGameView) RENDERER(strobeEffect) RENDERER(renderPixelDustView)
RENDERER(staticColor) RENDERER(patternRainbowGradient) RENDERER(patternRainbowLinearBand)
RENDERER(renderVideoPlayerView) RENDERER(renderMatrixRainView) RENDERER(renderMatrixFaceView)
RENDERER(displaySleepMode)
using ViewRenderFunc=void(*)();
'''

TEST = r'''
int main(){
  // Golden BLE view-ID contract, independent of the production dispatch table.
  const char* expected[]={
    "renderDebugSquaresView","renderLoadingBarView","patternPlasma","drawTransFlag","drawLGBTFlag",
    "renderFaceWithPlasma","renderFaceWithPlasma","renderFaceWithPlasma","renderFaceWithPlasma",
    "renderFaceWithPlasma","renderSpiralEyesView","renderPlasmaFaceView","renderFaceWithPlasma",
    "updateStarfield","renderBsodView","updateDVDLogos","renderFlameEffectView","renderFluidEffectView",
    "renderFaceWithPlasma","renderFullscreenSpiralPalette","renderFullscreenSpiralWhite",
    "renderScrollingTextView","renderDinoGameView","strobeEffect","renderPixelDustView","staticColor",
    "patternRainbowGradient","patternRainbowLinearBand","renderFaceWithPlasma","renderVideoPlayerView",
    "renderMatrixRainView","renderMatrixFaceView","renderFaceWithPlasma"
  };
  static_assert(sizeof(expected)/sizeof(expected[0])==TOTAL_VIEWS,"Update the stable view-ID contract deliberately");
  for(int view=0;view<TOTAL_VIEWS;++view){
    currentView=view;renderer=nullptr;blushState=BlushState::Inactive;
    displayCurrentView(view);
    assert(renderer && strcmp(renderer,expected[view])==0);
    assert(resetView==view && resets==unsigned(view+1));
    assert(micEnabled==viewUsesMic(view));
  }
  auto previous=resets;displayCurrentView(currentView);assert(resets==previous);
  assert(!viewNeedsContinuousRefresh(VIEW_TRANS_FLAG));assert(!viewNeedsContinuousRefresh(VIEW_STATIC_COLOR));
  assert(viewNeedsContinuousRefresh(VIEW_VIDEO_PLAYER));
  assert(viewFrameIntervalMillis(VIEW_DVD_LOGO)==runtime::kDvdFrameIntervalMs);
  assert(viewFrameIntervalMillis(VIEW_NORMAL_FACE)==LF_FACE_VIEW_FRAME_INTERVAL_MS);
  otaActive=true;assert(viewFrameBudgetMicros(VIEW_NORMAL_FACE)==LF_OTA_ACTIVE_FRAME_INTERVAL_MS*1000);
  otaActive=false;
  currentView=VIEW_BLUSH_FACE;displayCurrentView(currentView);
  assert(blushState==BlushState::FadeIn && isBlushFadingIn && blushStateStartTime==clockMs && blushes);
  pairing={true,true,123456};renderer=nullptr;auto before=presents;
  displayCurrentView(VIEW_NORMAL_FACE);assert(!renderer && overlays==1 && presents==before+1 && !headroom);
  sleepModeActive=true;displayCurrentView(VIEW_NORMAL_FACE);assert(strcmp(renderer,"displaySleepMode")==0 && !headroom);
  pairing={};sleepModeActive=false;renderer=nullptr;displayCurrentView(255);assert(!renderer);
  puts("All 33 view IDs, transitions, overlays and render pacing contracts passed.");
}
'''


class ViewManagerTest(unittest.TestCase):
    def test_production_dispatch_contract(self):
        source = (ROOT / "src/app/ViewManager.cpp").read_text(encoding="utf-8")
        table = source[source.index("static const ViewRenderFunc VIEW_RENDERERS"):source.index("bool viewNeedsPreClear(")]
        functions = [function(source, name) for name in
                     ("viewUsesMic", "viewNeedsContinuousRefresh", "viewFrameIntervalMillis",
                      "viewFrameBudgetMicros", "viewNeedsPreClear", "displayCurrentView")]
        with tempfile.TemporaryDirectory(prefix="lumifur-views-") as directory:
            cpp, executable = Path(directory) / "test.cpp", Path(directory) / "test.exe"
            cpp.write_text(STUB + table + "\n".join(functions) + TEST, encoding="utf-8")
            subprocess.run(["g++", "-std=c++17", f"-I{ROOT / 'src'}", f"-I{ROOT / 'include'}",
                            str(cpp), "-o", str(executable)], check=True)
            subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    unittest.main()
