#include "app/Sensors.h"
#include "app/AppStateSync.h"
#include "app/ViewState.h"
#include "config/RuntimeSettings.h"
#include "core/MotionState.h"
#include <Arduino.h>

Adafruit_LIS3DH accel;
bool g_accelerometer_initialized = false;
bool useShakeSensitivity = true;

namespace
{
motion::Sample sample;
uint32_t lastSampleMs = 0;

bool refresh(unsigned long interval)
{
  if (!accelerometerEnabled || !g_accelerometer_initialized) return false;
  const uint32_t now = millis();
  if (!sample.valid || now - lastSampleMs >= interval)
  {
    accel.read();
    sample.update(accel.x_g, accel.y_g, accel.z_g);
    lastSampleMs = now;
  }
  return sample.valid;
}
}

bool getLatestAcceleration(float &x, float &y, float &z, unsigned long interval)
{
  AppStateLock stateLock; // Also serializes the I2C transaction with other sensors.
  if (!refresh(interval)) return false;
  x = sample.x; y = sample.y; z = sample.z;
  return true;
}

float getCurrentThreshold() { return motion::threshold(useShakeSensitivity); }

bool detectMotion()
{
  AppStateLock stateLock;
  if (!refresh(useShakeSensitivity ? MOTION_SAMPLE_INTERVAL_FAST : MOTION_SAMPLE_INTERVAL_SLOW)) return false;
  if (currentView == VIEW_PIXEL_DUST) return false;
  return useShakeSensitivity ? sample.shake : sample.activity;
}

bool sleepMotionDetected()
{
  AppStateLock stateLock;
  return refresh(MOTION_SAMPLE_INTERVAL_SLOW) && sample.activity;
}
