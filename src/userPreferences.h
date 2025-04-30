#pragma once
#include <Preferences.h>

static Preferences prefs;
static constexpr char NAMESPACE[] = "LumiFur";
static constexpr char KEY_LAST[]  = "lastView";

// Returns a reference to a static Preferences instance.
Preferences& getPrefs() {
    static Preferences prefs;
    return prefs;
  }
  
  // Initializes the Preferences system; call this in setup().
  void initPreferences() {
    // Open the "LumiFur" namespace in read/write mode.
    getPrefs().begin("LumiFur", false);
  }
  
  // Debug mode functions
  bool getDebugMode() {
    return getPrefs().getBool("debug", true);
  }
  void setDebugMode(bool debugMode) {
    getPrefs().putBool("debug", debugMode);
  }
  
  // Brightness functions
  int getUserBrightness() {
    return getPrefs().getInt("brightness", 204);
  }
  void setUserBrightness(int brightness) {
    getPrefs().putInt("brightness", brightness);
  }
  
  // Blink mode functions
  bool getBlinkMode() {
    return getPrefs().getBool("blinkmode", true);
  }
  void setBlinkMode(bool blinkMode) {
    getPrefs().putBool("blinkmode", blinkMode);
  }
  
  // Sleep mode functions
  bool getSleepMode() {
    return getPrefs().getBool("sleepmode", true);
  }
  void setSleepMode(bool sleepMode) {
    getPrefs().putBool("sleepmode", sleepMode);
  }
  
  // Dizzy mode functions
  bool getDizzyMode() {
    return getPrefs().getBool("dizzymode", true);
  }
  void setDizzyMode(bool dizzyMode) {
    getPrefs().putBool("dizzymode", dizzyMode);
  }

  // Auto brightness functions
  bool getAutoBrightness() {
    return getPrefs().getBool("autobrightness", true);
  }
  void setAutoBrightness(bool autoBrightness) {
    getPrefs().putBool("autobrightness", autoBrightness);
  }

  // Accelerometer functions
  bool getAccelerometerEnabled() {
    return getPrefs().getBool("accelerometer", true);
  }
  void setAccelerometerEnabled(bool enabled) {
    getPrefs().putBool("accelerometer", enabled);
  }

  // Aurora mode functions
  bool getAuroraMode() {
    return getPrefs().getBool("auroramode", true);
  }
  void setAuroraMode(bool enabled) {
    getPrefs().putBool("auroramode", enabled);
  }

  uint8_t getLastView() {
    return getPrefs().getUChar(KEY_LAST, 0);
  }
  
  void saveLastView(uint8_t v) {
    getPrefs().putUChar(KEY_LAST, v);
  }


  // Clear all preferences - Implement feature to reset controller to default settings
  void clearPreferences() {
    getPrefs().clear();
  }
  
uint8_t getLastView();            // returns 0..totalViews-1 (default 0)
void    saveLastView(uint8_t v); // store the last selected view