#pragma once
#include "core/AnimationState.h"
using animation::AnimationState;
using animation::BlinkTimings;
using animation::BlushState;

extern AnimationState &gAnimationState;

extern BlinkTimings &blinkState;
extern unsigned long &lastEyeBlinkTime;
extern unsigned long &nextBlinkDelay;
extern int &blinkProgress;
extern bool &isBlinking;
extern bool &manualBlinkTrigger;
extern bool &isEyeBouncing;
extern unsigned long &eyeBounceStartTime;
extern volatile int &currentEyeYOffset;
extern int &eyeBounceCount;
extern uint8_t &viewBeforeEyeBounce;
extern volatile int &idleEyeYOffset;
extern volatile int &idleEyeXOffset;
extern unsigned long &spiralStartMillis;
extern int &previousView;
extern volatile BlushState &blushState;
extern volatile unsigned long &blushStateStartTime;
extern bool &isBlushFadingIn;
extern uint8_t &originalViewBeforeBlush;
extern bool &wasBlushOverlay;
extern volatile uint8_t &blushBrightness;

void initBlinkState();
void updateIdleHoverAnimation();
void updateEyeBounceAnimation();
void updateBlinkAnimation();
void disableBlush();
void updateBlush();
void updateViewAnimations(unsigned long nowMs);
