#pragma once
#include <Preferences.h>

static constexpr char NAMESPACE[] = "LumiFur";
static constexpr char KEY_LAST[] = "lastView";

// Returns a reference to a static Preferences instance.
inline Preferences &getPrefs()
{
  static Preferences prefs;
  return prefs;
}

// Initializes the Preferences system; call this in setup().
inline void initPreferences()
{
  // Open the "LumiFur" namespace in read/write mode.
  getPrefs().begin("LumiFur", false);
}

// Debug mode functions
inline bool getDebugMode()
{
  return getPrefs().getBool("debug", true);
}
inline void setDebugMode(bool debugMode)
{
  getPrefs().putBool("debug", debugMode);
}

// Brightness functions
inline int getUserBrightness()
{
  return getPrefs().getInt("brightness", 255); // Default to 255 (full brightness)
}
inline void setUserBrightness(int brightness)
{
  getPrefs().putInt("brightness", brightness);
}

// Blink mode functions
inline bool getBlinkMode()
{
  return getPrefs().getBool("blinkmode", true);
}
inline void setBlinkMode(bool blinkMode)
{
  getPrefs().putBool("blinkmode", blinkMode);
}

// Sleep mode functions
inline bool getSleepMode()
{
  return getPrefs().getBool("sleepmode", true);
}
inline void setSleepMode(bool sleepMode)
{
  getPrefs().putBool("sleepmode", sleepMode);
}

// Dizzy mode functions
inline bool getDizzyMode()
{
  return getPrefs().getBool("dizzymode", true);
}
inline void setDizzyMode(bool dizzyMode)
{
  getPrefs().putBool("dizzymode", dizzyMode);
}

// Auto brightness functions
inline bool getAutoBrightness()
{
  return getPrefs().getBool("autobrightness", true);
}
inline void setAutoBrightness(bool autoBrightness)
{
  getPrefs().putBool("autobrightness", autoBrightness);
}

// Accelerometer functions
inline bool getAccelerometerEnabled()
{
  return getPrefs().getBool("accelerometer", true);
}
inline void setAccelerometerEnabled(bool enabled)
{
  getPrefs().putBool("accelerometer", enabled);
}

// Aurora mode functions
inline bool getAuroraMode()
{
  return getPrefs().getBool("auroramode", true);
}
inline void setAuroraMode(bool enabled)
{
  getPrefs().putBool("auroramode", enabled);
}

inline bool getStaticColorMode()
{
  return getPrefs().getBool("staticcolormode", false);
}
inline void setStaticColorMode(bool enabled)
{
  getPrefs().putBool("staticcolormode", enabled);
}

inline uint8_t getLastView()
{
  return getPrefs().getUChar(KEY_LAST, 0);
}

inline void saveLastView(uint8_t v)
{
  getPrefs().putUChar(KEY_LAST, v);
}

inline void saveUserText(const String &text)
{
  getPrefs().putString("userText", text);
}

inline String getUserText()
{
  return getPrefs().getString("userText", "");
}

inline void saveScrollSpeed(uint16_t speed)
{
  getPrefs().putUShort("scrollSpeed", speed);
}

inline uint16_t getScrollSpeed()
{
  return getPrefs().getUShort("scrollSpeed", 4);
}

inline void saveSelectedColor(const String &text)
{
  getPrefs().putString("selectedColor", text);
}

inline String getSelectedColor()
{
  return getPrefs().getString("selectedColor", "");
}

// Clear all preferences - Implement feature to reset controller to default settings
inline void clearPreferences()
{
  getPrefs().clear();
}
