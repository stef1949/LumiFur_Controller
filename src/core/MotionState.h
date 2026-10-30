#pragma once
#include <cmath>

namespace motion
{
constexpr float kSleepThreshold = 1.0f;
constexpr float kShakeThreshold = 1.0f;
inline float threshold(bool shake) { return shake ? kShakeThreshold : kSleepThreshold; }
inline float delta(float current, float previous) { return std::fabs(current - previous); }
inline bool aboveThreshold(float x, float y, float z, float limit)
{
  return x > limit || y > limit || z > limit;
}

struct Sample
{
  float x = 0, y = 0, z = 0;
  float deltaX = 0, deltaY = 0, deltaZ = 0;
  bool valid = false;
  bool shake = false;
  bool activity = false;

  void update(float newX, float newY, float newZ)
  {
    // Establish a baseline on the first sample; startup is not movement.
    deltaX = valid ? delta(newX, x) : 0;
    deltaY = valid ? delta(newY, y) : 0;
    deltaZ = valid ? delta(newZ, z) : 0;
    x = newX; y = newY; z = newZ;
    valid = true;
    shake = aboveThreshold(deltaX, deltaY, deltaZ, kShakeThreshold);
    activity = aboveThreshold(deltaX, deltaY, deltaZ, kSleepThreshold);
  }
};
}
