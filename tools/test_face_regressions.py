"""Run production face/boop functions with a native mock display: python tools/test_face_regressions.py."""
from pathlib import Path
import subprocess
import tempfile
import re

ROOT = Path(__file__).resolve().parents[1]
main = (ROOT / "src/app/main.cpp").read_text(encoding="utf-8")


def function(source, signature):
    start = source.index(signature)
    brace = source.index("{", start)
    while ";" in source[start:brace]:
        start = source.index(signature, start + 1)
        brace = source.index("{", start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


stub = r'''
#include <cstdint>
#include <cstring>
#include <cmath>
#include <cassert>
#include <cstdio>
#include <new>
#define pgm_read_byte(p) (*(p))
#define BUTTON_UP 1
#define BUTTON_DOWN 2
#define PROFILE_SECTION(x)
unsigned long clockMs = 100;
unsigned long millis() { return clockMs; }
long random(long n) { return n > 0 ? 1 % n : 0; }
long random(long a, long b) { return a + random(b-a); }
struct { void println(const char*) {} template<class... T> void printf(const char*, T...) {} } Serial;
struct DisplayOutputType {
  uint16_t pixels[4096] = {};
  int width() { return 128; } int height() { return 32; }
  uint16_t color565(uint8_t r,uint8_t g,uint8_t b) { return (r>>3)<<11 | (g>>2)<<5 | (b>>3); }
  void drawPixel(int x,int y,uint16_t c) { if(x>=0 && x<128 && y>=0 && y<32) pixels[y*128+x]=c; }
  void fillScreen(uint16_t c) { for(auto &v:pixels) v=c; }
} display;
auto dma_display = &display;
uint16_t globalBrightnessScaleFixed = 256, faceScale = 256;
struct CRGB { uint8_t r=0,g=0,b=0; CRGB(){} CRGB(int a,int c,int d):r(a),g(c),b(d){} static const CRGB Black; };
const CRGB CRGB::Black{};
struct { CRGB color{255,0,0}; } gStaticColorState;
bool staticColorModeEnabled=false, matrixRainInsteadOfPlasma=true;
void ensureStaticColorLoaded() {}
CRGB getStaticColorCached() { return gStaticColorState.color; }
uint16_t getNormalFaceSoftwareBrightnessScale() { return faceScale; }
int time_counter=0, currentPalette=0;
uint8_t sin8(float) { return 100; } uint8_t cos8(float) { return 100; }
CRGB ColorFromPalette(int,int) { return CRGB(0,255,0); }
const CRGB* getScaledPlasmaPaletteLut(uint16_t) { static CRGB p[256]; return p; }
void drawPixelRgbFast(int x,int y,uint8_t r,uint8_t g,uint8_t b) { display.drawPixel(x,y,display.color565(r,g,b)); }
uint16_t matrixRainColorAt(int,int,uint16_t=UINT16_MAX);
void drawMatrixRainThroughXbm(int,int,int,int,const uint8_t*,uint16_t=UINT16_MAX);
void matrixRainAdvanceColumns();
unsigned long boopHits[16]={}, boopRevertAt=0, lastActivityTime=0, blushStateStartTime=0;
uint8_t boopHitCount=0,boopSavedView=0,userSelectedView=2,currentView=2,boopMode=3,originalViewBeforeBlush=0;
bool boopLoadingActive=false,wasBlushOverlay=false,sleepModeActive=false;
#include "core/AnimationState.h"
using animation::BlushState;
BlushState &blushState=animation::state().blushState;
struct AppStateLock {};
constexpr unsigned long fadeInDuration=2000,fullDuration=6000,fadeOutDuration=2000;
#include "app/Views.h"
void notifyBleTask() {} void requestDisplayRefresh() {} void wakeFromSleepMode() {}
uint8_t persisted=0; void scheduleLastViewPersist(uint8_t v) { persisted=v; }
struct BleWorkItem { unsigned length; uint8_t data[1]; };
constexpr unsigned long PAIRING_MODE_HOLD_MS=3000,PAIRING_MODE_WINDOW_MS=60000,BUTTON_DEBOUNCE_MS=50,PAIRING_RESET_HOLD_MS=3000;
#define LOW 0
bool keyPressed[3]={}; int digitalRead(int pin){return keyPressed[pin]?0:1;}
void resetBlePairing() {}
int pairingCount=0;
void noteButtonInteraction(unsigned long) {} void refreshBleAdvertising() {}
void startPairingMode(unsigned long) { ++pairingCount; }
'''

assert "matrixRainAdvance();" in function(main, "static void renderMatrixFaceView()")

effect = (ROOT / "src/effects/matrixRainEffect.cpp").read_text(encoding="utf-8")
effect = "\n".join(line for line in effect.splitlines() if not line.startswith("#include"))
buttons = '#include "core/InputState.h"\nusing namespace input;\n'
view_state = (ROOT / "src/app/ViewState.cpp").read_text(encoding="utf-8")
inputs = (ROOT / "src/app/InputController.cpp").read_text(encoding="utf-8")
controls = (ROOT / "src/ble/ble_control.cpp").read_text(encoding="utf-8")
angry = main[main.index("    case VIEW_ANGRY_FACE:", main.index("void blinkingEyes()")):]
angry = angry[angry.index("{")+1:angry.index("      return;")]
font_header = (ROOT / "src/assets/fonts/matrixCodeFont.h").read_text(encoding="utf-8").replace("#include <Arduino.h>", "")
font_data = (ROOT / "src/assets/fonts/matrixCodeFont.cpp").read_text(encoding="utf-8").replace('#include "assets/fonts/matrixCodeFont.h"', "")
eye = re.search(r"static uint8_t Eye\[\] = \{.*?\};", (ROOT / "src/assets/bitmaps.h").read_text(encoding="utf-8"), re.S).group()
parts = [stub, "#define PROGMEM\n#define memcpy_P memcpy\n", font_header, font_data, effect, buttons, eye]
parts += [function(view_state, "void selectUserView("), function(view_state, "void handleBoop("), function(inputs,"void handleButtonInputs(")]
parts += [function(main, name) for name in ["void drawPlasmaXbm(", "void drawBitmapAdvanced("]]
parts += [function(controls, "static void handleBleFaceWriteWork(")]
parts += ["void angryEyes() {"+angry+"}"]
parts.append(r'''
int main() {
  for(int hit=1;hit<=40;++hit) {
    handleBoop(100+hit*100);
    if(hit<5) assert(currentView==2);
    else if(hit<10) assert(currentView==VIEW_SPIRAL_EYES);
    else if(hit<15) assert(currentView==VIEW_ANGRY_FACE);
    else assert(currentView==VIEW_LOADING_BAR);
  }
  assert(boopHitCount==16);
  handleBoop(30000); assert(boopHitCount==1);
  boopRevertAt=50000; currentView=VIEW_LOADING_BAR;
  handleBleFaceWriteWork({1,{7}}); assert(currentView==7 && boopRevertAt==0 && userSelectedView==7);
  boopRevertAt=50000;
  handleBleFaceWriteWork({1,{7}}); assert(boopRevertAt==0 && persisted==7);
  boopRevertAt=50000; handleBleFaceWriteWork({1,{255}}); assert(boopRevertAt==50000);
  keyPressed[BUTTON_DOWN]=true;
  handleButtonInputs(100); handleButtonInputs(150); handleButtonInputs(3149); assert(pairingCount==0);
  handleButtonInputs(3150); assert(pairingCount==1);
  handleButtonInputs(3160); assert(pairingCount==1);
  keyPressed[BUTTON_DOWN]=false; handleButtonInputs(3200); handleButtonInputs(3250);
  currentView=7; boopRevertAt=50000;
  keyPressed[BUTTON_DOWN]=true; handleButtonInputs(3300); handleButtonInputs(3350);
  keyPressed[BUTTON_DOWN]=false; handleButtonInputs(3400); handleButtonInputs(3450);
  assert(currentView==6 && userSelectedView==6 && boopRevertAt==0);
  boopRevertAt=50000;
  keyPressed[BUTTON_UP]=true; handleButtonInputs(3500); handleButtonInputs(3550);
  keyPressed[BUTTON_UP]=false; handleButtonInputs(3600); handleButtonInputs(3650);
  assert(currentView==7 && userSelectedView==7 && boopRevertAt==0);
  angryEyes(); assert(display.pixels[6*128+4]!=0);
  staticColorModeEnabled=true; angryEyes(); assert(display.pixels[6*128+4]==display.color565(255,0,0));
  staticColorModeEnabled=false;
  initMatrixRainEffect(&display);
  for(int i=0;i<g_numCols;++i) { g_cols[i].y=16; g_cols[i].tick=0; g_cols[i].length=20; }
  matrixRainBeginFrame(); matrixRainAdvance(); assert(g_rainMapValid);
  auto tick = g_cols[0].tick;
  ++clockMs; matrixRainAdvanceColumns();
  ++clockMs; matrixRainAdvanceColumns();
  assert(g_cols[0].tick == tick && g_rainMapValid);
  matrixRainBeginFrame(); matrixRainAdvanceColumns();
  assert(g_cols[0].tick == tick + 1 && !g_rainMapValid);
  const uint8_t bitmap[16]={255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255};
  for(bool bypass:{false,true}) {
    display.fillScreen(0); drawPlasmaXbm(0,0,8,16,bitmap,0,5,.2,0,bypass);
    for(auto p:display.pixels) assert(p==0);
    drawPlasmaXbm(0,0,8,16,bitmap,0,5,.2,255,bypass);
    bool lit=false; for(auto p:display.pixels) lit|=p!=0; assert(lit);
  }
  display.fillScreen(0); drawBitmapAdvanced(0,0,8,16,bitmap,0,0,true);
  uint16_t open[4096]; memcpy(open,display.pixels,sizeof(open));
  display.fillScreen(0); drawBitmapAdvanced(0,0,8,16,bitmap,0,100,true);
  assert(memcmp(open,display.pixels,sizeof(open))!=0);
  display.fillScreen(0); drawBitmapAdvanced(0,0,32,16,Eye,0,0,true);
  memcpy(open,display.pixels,sizeof(open));
  display.fillScreen(0); drawBitmapAdvanced(0,0,32,16,Eye,0,100,true);
  assert(memcmp(open,display.pixels,sizeof(open))!=0);
  globalBrightnessScaleFixed=64;
  assert(matrixRainColorAt(0,10,256)!=matrixRainColorAt(0,10));
  assert(matrixRainColorAt(0,10,0)==0);
  puts("Face regressions passed: boop thresholds/history, BLE/button cancellation, pairing, angry eyes, matrix brightness/blinking/map.");
}
''')

(ROOT / ".pio").mkdir(exist_ok=True)
with tempfile.TemporaryDirectory(prefix="lumifur-regressions-", dir=ROOT / ".pio") as directory:
    source = Path(directory) / "regressions.cpp"
    executable = Path(directory) / "regressions.exe"
    source.write_text("\n".join(parts), encoding="utf-8")
    subprocess.run(["g++", "-std=c++17", "-include", "initializer_list", "-I"+str(ROOT/"src"), "-I"+str(ROOT/"include"), str(source), str(ROOT/"src/core/mic/mic_math.cpp"), str(ROOT/"src/core/AnimationState.cpp"), "-include", str(ROOT/"src/core/mic/mic_math.h"), "-o", str(executable)], check=True)
    subprocess.run([str(executable)], check=True)

