#include "rendering/RenderSupport.h"
#include "rendering/EffectRenderer.h"
#include "rendering/ScrollRenderer.h"
#include "app/AppSetup.h"
#include "app/ViewManager.h"
#include "ble/ble_control.h"
#include "ble/BleStatus.h"
#include "config/ViewPersistence.h"
#include "core/InternalTemperature.h"
#include <FFat.h>

namespace {
constexpr unsigned BAUD_RATE = 115200;
bool gVideoStorageReady = false;
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
void err(int x)
{
  uint8_t i;
  pinMode(LED_BUILTIN, OUTPUT); // Using onboard LED
  for (int i = 1;; i++)
  {                                   // Loop forever...
    digitalWrite(LED_BUILTIN, i & 1); // LED on/off blink to alert user
    delay(x);
  }
}


}

void initializeApplication()
{
  initializeAppStateSync();
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

  if (!internalTemperatureInit())
  {
    Serial.printf("Temperature sensor unavailable using %s; BLE temperature updates disabled.\n",
                  internalTemperatureDriverName());
  }

  initPreferences(); // Initialize Preferences
  autoBrightnessFloor = getAutoBrightnessFloor();
  userBrightness = static_cast<uint8_t>(constrain(getUserBrightness(), 0, 255));
  autoBrightnessEnabled = getAutoBrightness();
  mouthMicBrightnessOverrideEnabled = getMouthMicBrightnessOverride();
  disableBleIndicatorLight = getDisableBleIndicatorLight();
  disableBleStatusIcon = getDisableBleStatusIcon();
  waveshareBrightnessBoost = getWaveshareBrightnessBoost(); // Software gain; scan timing is independent of the saved flag.
  syncBrightnessState(userBrightness);
  accelerometerEnabled = getAccelerometerEnabled();
  sleepModeEnabled = getSleepMode();
  auroraModeEnabled = getAuroraMode();
  matrixRainInsteadOfPlasma = getMatrixRainInsteadOfPlasma();
  boopMode = getBoopMode();
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
  userSelectedView = currentView;
  animation::state().previousView = currentView;
  initializeViewPersistence(currentView);
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

  setupBleControl();

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

  mxconfig.driver = HUB75_I2S_CFG::FM6126A; // Default for panels using FM6126A chips
  
  // A saved brightness boost must never reduce the visual scan rate. Keep
  // the same stable bus timing for both settings; boost is software gain only.
  mxconfig.min_refresh_rate = LF_HUB75_MIN_REFRESH_RATE_HZ;
  mxconfig.i2sspeed = HUB75_I2S_CFG::HZ_16M;

  mxconfig.latch_blanking = LF_HUB75_LATCH_BLANKING;        // Keep blanking explicit to avoid per-panel surprises.
  mxconfig.clkphase = LF_HUB75_CLKPHASE;                    // false selects the library's negative-edge clocking mode.
  mxconfig.double_buff = true;
  mxconfig.setPixelColorDepthBits(LF_HUB75_COLOR_DEPTH_BITS);

#ifndef VIRTUAL_PANE
  dma_display = new MatrixPanel_I2S_DMA(mxconfig);
  dma_display->begin();
  updateGlobalBrightnessScale(userBrightness);
  syncBrightnessState(userBrightness);
  initFlameEffect(dma_display);
  initMatrixRainEffect(dma_display);   // ADD — this branch is what MatrixPortal uses
#else
  chain = new MatrixPanel_I2S_DMA(mxconfig);
  chain->begin();
  updateGlobalBrightnessScale(userBrightness);
  syncBrightnessState(userBrightness);
  matrix = new VirtualMatrixPanel((*chain), NUM_ROWS, NUM_COLS, PANEL_WIDTH, PANEL_HEIGHT, CHAIN_TOP_LEFT_DOWN);
  initFlameEffect(matrix);
  initMatrixRainEffect(matrix);        // already present (~line 4270)
#endif

  dma_display->clearScreen();
  presentDisplayFrame();

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

  createEffectRenderers(gVideoStorageReady);

  lastActivityTime = millis(); // Initialize the activity timer for sleep mode

  randomSeed(analogRead(0)); // Seed the random number generator

  // Set sleep timeout based on debug mode
  SLEEP_TIMEOUT_MS = DEBUG_MODE ? SLEEP_TIMEOUT_MS_DEBUG : SLEEP_TIMEOUT_MS_NORMAL;

  // Set initial plasma color palette
  currentPalette = RainbowColors_p;

  snprintf(txt, sizeof(txt), "%s", getUserText().c_str());
  initializeScrollingText();

  initializeEffectAnimations();

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
    presentDisplayFrame();
    loadingProgress++;
    delay(15); // ~1.5 s total at loadingMax=100
  }
  // #endif

  // Spawn BLE notify task (you already have):
  if (!startBleWorker())
  {
    Serial.println("Failed to start BLE worker");
    err(125);
  }

  if (!startBleNotifications() || !startViewManager())
  {
    Serial.println("Failed to start application tasks");
    err(125);
  }
}
