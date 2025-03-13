#include <Preferences.h>

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
  