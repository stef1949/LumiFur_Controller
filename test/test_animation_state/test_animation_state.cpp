#include <unity.h>
#include "core/AnimationState.h"

using namespace animation;

void setUp(void)
{
  // Set up before each test
}

void tearDown(void)
{
  // Clean up after each test
}

void test_animation_state_initialization(void)
{
  AnimationState &s = state();
  
  // Test initial values
  TEST_ASSERT_EQUAL_UINT(0, s.lastEyeBlinkTime);
  TEST_ASSERT_EQUAL_UINT(1000, s.nextBlinkDelay);
  TEST_ASSERT_EQUAL_INT(0, s.blinkProgress);
  TEST_ASSERT_FALSE(s.isBlinking);
  TEST_ASSERT_FALSE(s.manualBlinkTrigger);
}

void test_blink_timings_initialization(void)
{
  BlinkTimings timings;
  
  TEST_ASSERT_EQUAL_UINT(0, timings.startTime);
  TEST_ASSERT_EQUAL_UINT(700, timings.durationMs);
  TEST_ASSERT_EQUAL_UINT(50, timings.closeDuration);
  TEST_ASSERT_EQUAL_UINT(20, timings.holdDuration);
  TEST_ASSERT_EQUAL_UINT(50, timings.openDuration);
}

void test_animation_state_reset(void)
{
  AnimationState &s = state();
  
  // Modify some values
  s.lastEyeBlinkTime = 1000;
  s.isBlinking = true;
  s.blinkProgress = 50;
  s.isEyeBouncing = true;
  s.currentMaw = 5;
  
  // Reset the state
  reset(s);
  
  // Verify reset worked
  TEST_ASSERT_EQUAL_UINT(0, s.lastEyeBlinkTime);
  TEST_ASSERT_EQUAL_INT(0, s.blinkProgress);
  TEST_ASSERT_FALSE(s.isBlinking);
  TEST_ASSERT_FALSE(s.isEyeBouncing);
  TEST_ASSERT_EQUAL_INT(1, s.currentMaw);
}

void test_blush_state_initialization(void)
{
  AnimationState &s = state();
  reset(s);
  
  TEST_ASSERT_EQUAL_INT(static_cast<int>(BlushState::Inactive), static_cast<int>(s.blushState));
  TEST_ASSERT_EQUAL_UINT(0, s.blushStateStartTime);
  TEST_ASSERT_FALSE(s.isBlushFadingIn);
  TEST_ASSERT_EQUAL_UINT8(0, s.blushBrightness);
}

void test_eye_bounce_state(void)
{
  AnimationState &s = state();
  reset(s);
  
  // Test eye bounce initial state
  TEST_ASSERT_FALSE(s.isEyeBouncing);
  TEST_ASSERT_EQUAL_UINT(0, s.eyeBounceStartTime);
  TEST_ASSERT_EQUAL_INT(0, s.currentEyeYOffset);
  TEST_ASSERT_EQUAL_INT(0, s.eyeBounceCount);
}

void test_maw_state_initialization(void)
{
  AnimationState &s = state();
  reset(s);
  
  TEST_ASSERT_EQUAL_INT(1, s.currentMaw);
  TEST_ASSERT_EQUAL_UINT(0, s.mawChangeTime);
  TEST_ASSERT_FALSE(s.mawTemporaryChange);
  TEST_ASSERT_FALSE(s.mouthOpen);
  TEST_ASSERT_EQUAL_UINT(0, s.lastMouthTriggerTime);
}

void test_idle_hover_offsets(void)
{
  AnimationState &s = state();
  reset(s);
  
  TEST_ASSERT_EQUAL_INT(0, s.idleEyeYOffset);
  TEST_ASSERT_EQUAL_INT(0, s.idleEyeXOffset);
}

void setup()
{
  UNITY_BEGIN();
  
  RUN_TEST(test_animation_state_initialization);
  RUN_TEST(test_blink_timings_initialization);
  RUN_TEST(test_animation_state_reset);
  RUN_TEST(test_blush_state_initialization);
  RUN_TEST(test_eye_bounce_state);
  RUN_TEST(test_maw_state_initialization);
  RUN_TEST(test_idle_hover_offsets);
  
  UNITY_END();
}

void loop()
{
  // Unity tests run once
}
