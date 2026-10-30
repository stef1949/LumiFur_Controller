#include "config/RuntimeSettings.h"
#include "app/AppRuntime.h"
#include "app/PowerManagement.h"
#include "core/AdaptiveBrightness.h"
#include "config/debug_config.h"

bool autoBrightnessEnabled = true;
bool accelerometerEnabled = true;
bool sleepModeEnabled = true;
bool auroraModeEnabled = true;
bool staticColorModeEnabled = false;
bool mouthMicBrightnessOverrideEnabled = false;
bool disableBleIndicatorLight = false; // byte 6, DEBUG_DISABLE_BLE_INDICATOR_LIGHT
bool disableBleStatusIcon = false;     // byte 7, DEBUG_DISABLE_BLE_STATUS_ICON
bool waveshareBrightnessBoost = false; // byte 8, DEBUG_ENABLE_BRIGTHNESS_BOOST_WAVESHARE
bool constantColorConfig = false;
CRGB constantColor = CRGB::Green; // Default color for constant color mode
bool configApplyAutoBrightness = true; // Variable to track auto brightness application
bool configApplySleepMode = false;
bool configApplyAuroraMode = false;
bool configApplyAccelerometer = true;
bool configApplyConstantColor = false; // Variable to track constant color application
bool matrixRainInsteadOfPlasma = false;
uint8_t autoBrightnessFloor = 15;
uint8_t boopMode = 0;
uint8_t userBrightness = 255;

void applyConfigOptions()
{
  #if DEBUG_MODE
  Serial.println("Applying configuration options...");
#endif
  if (autoBrightnessEnabled)
  {
    #if DEBUG_MODE
    Serial.println("Auto Brightness enabled: adjusting brightness automatically.");
    #endif
    // Example: call a function to update auto brightness (if implemented)
    // updateAutoBrightness();
    configApplyAutoBrightness = true;
  }
  else
  {
    #if DEBUG_MODE
    Serial.println("Auto brightness disabled. Applying user-set brightness.");
    #endif
    updateGlobalBrightnessScale(userBrightness);
    syncBrightnessState(userBrightness);
    #if DEBUG_MODE
    Serial.printf("Applied manual brightness: %u\n", userBrightness);
    #endif
  }

  if (accelerometerEnabled)
  {
    #if DEBUG_MODE
    Serial.println("Accelerometer enabled.");
    #endif
    // Insert code to enable accelerometer-driven actions here.
    configApplyAccelerometer = true;
  }
  else
  {
    #if DEBUG_MODE
    Serial.println("Accelerometer disabled.");
    #endif
    // Optionally disable or ignore accelerometer actions.
    configApplyAccelerometer = false;
  }

  if (sleepModeEnabled)
  {
    #if DEBUG_MODE
    Serial.println("Sleep mode enabled: device can enter sleep mode.");
    #endif
    // Update sleep-related thresholds or enable sleep mode triggers.
    configApplySleepMode = true;
  }
  else
  {
    #if DEBUG_MODE
    Serial.println("Sleep mode disabled: forcing device to remain awake.");
    #endif
    configApplySleepMode = false;
    wakeFromSleepMode();
  }

  if (staticColorModeEnabled)
  {
    #if DEBUG_MODE
    Serial.println("Static color mode enabled: using user-selected color.");
    #endif
    constantColor = getStaticColorCached();
    constantColorConfig = true;
    configApplyConstantColor = true;
    configApplyAuroraMode = false;
  }
  else
  {
    constantColorConfig = false;
    configApplyConstantColor = false;
    if (auroraModeEnabled)
    {
      #if DEBUG_MODE
      Serial.println("Aurora mode enabled: switching to aurora palette.");
      #endif
      // Assume auroraPalette and defaultPalette are defined globally.
      // currentPalette = auroraPalette;
      configApplyAuroraMode = true;
    }
    else
    {
      #if DEBUG_MODE
      Serial.println("Aurora mode disabled: using default palette.");
      #endif
      // currentPalette = defaultPalette;
      configApplyAuroraMode = false;
    }
  }
}

