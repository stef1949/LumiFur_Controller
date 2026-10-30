#pragma once

// Rendering, input updates and BLE controls share the same recursive lock.
// Release it before sleeping or waiting for task notifications. Never acquire
// it from an ISR or while holding a portMUX critical section.
void initializeAppStateSync();

class AppStateLock
{
public:
  AppStateLock();
  ~AppStateLock();
  void unlock();
  AppStateLock(const AppStateLock &) = delete;
  AppStateLock &operator=(const AppStateLock &) = delete;

private:
  bool locked_ = false;
};
