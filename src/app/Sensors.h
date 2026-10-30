#pragma once
#include <Adafruit_LIS3DH.h>

extern Adafruit_LIS3DH accel;
extern bool g_accelerometer_initialized;
extern bool useShakeSensitivity;
constexpr unsigned long MOTION_SAMPLE_INTERVAL_FAST = 15;
constexpr unsigned long MOTION_SAMPLE_INTERVAL_SLOW = 120;

bool getLatestAcceleration(float &x, float &y, float &z, unsigned long sampleInterval);
bool detectMotion();
bool sleepMotionDetected();
float getCurrentThreshold();
