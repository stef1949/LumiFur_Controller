#pragma once

inline float easeInOutQuad(float t)
{
  return t < 0.5f ? 2.0f * t * t : -1.0f + (4.0f - 2.0f * t) * t;
}
inline float easeInQuad(float t)
{
  if (t < 0.0f) return 0.0f;
  if (t > 1.0f) return 1.0f;
  return t * t;
}
inline float easeOutQuad(float t)
{
  if (t < 0.0f) return 0.0f;
  if (t > 1.0f) return 1.0f;
  return 1.0f - (1.0f - t) * (1.0f - t);
}
