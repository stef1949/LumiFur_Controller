#pragma once

inline bool hasElapsedSince(unsigned long now, unsigned long last, unsigned long interval)
{
  return static_cast<unsigned long>(now - last) >= interval;
}

template <typename Callback>
inline void runIfElapsed(unsigned long now, unsigned long &last, unsigned long interval, Callback &&callback)
{
  if (hasElapsedSince(now, last, interval))
  {
    last = now;
    callback();
  }
}
