#pragma once
#include "app/Views.h"
#include <cstdint>

// All mutable fields are protected by AppStateLock.
extern uint8_t currentView;
extern uint8_t userSelectedView;
extern unsigned long boopRevertAt;
extern bool boopLoadingActive;
constexpr unsigned long fadeInDuration = 2000;
constexpr unsigned long fullDuration = 6000;
constexpr unsigned long fadeOutDuration = 2000;
void selectUserView(uint8_t view);
void handleBoop(unsigned long now);
void updateTemporaryView(unsigned long now);

void updateViewTimers(unsigned long nowMs);
