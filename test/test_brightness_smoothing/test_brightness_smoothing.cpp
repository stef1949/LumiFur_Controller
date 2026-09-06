#include <unity.h>
#include "core/BrightnessSmoothing.h"

void setUp() {}
void tearDown() {}

static float transition(float start, float target, uint32_t durationMs, uint32_t stepMs)
{
  for (uint32_t elapsed = 0; elapsed < durationMs; elapsed += stepMs)
  {
    start = brightness::smoothAutoBrightness(start, target, stepMs);
  }
  return start;
}

void test_new_lux_target_does_not_cause_a_brightness_jump()
{
  const float brighter = brightness::smoothAutoBrightness(15.0f, 255.0f, 20);
  const float dimmer = brightness::smoothAutoBrightness(255.0f, 15.0f, 20);
  TEST_ASSERT_FLOAT_WITHIN(3.0f, 21.0f, brighter);
  TEST_ASSERT_FLOAT_WITHIN(3.0f, 249.0f, dimmer);
}

void test_transition_speed_is_independent_of_loop_cadence()
{
  const float fast = transition(15.0f, 255.0f, 800, 10);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, fast, transition(15.0f, 255.0f, 800, 20));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, fast, transition(15.0f, 255.0f, 800, 40));
  TEST_ASSERT_FLOAT_WITHIN(1.0f, 167.0f, fast);
}

void test_fades_are_monotonic_and_reach_both_integer_endpoints()
{
  for (int direction = 0; direction < 2; ++direction)
  {
    float current = direction == 0 ? 15.0f : 255.0f;
    const float target = direction == 0 ? 255.0f : 15.0f;
    for (int step = 0; step < 300; ++step)
    {
      const float next = brightness::smoothAutoBrightness(current, target, 20);
      TEST_ASSERT_TRUE(next >= 15.0f && next <= 255.0f);
      TEST_ASSERT_TRUE(direction == 0 ? next >= current : next <= current);
      current = next;
    }
    TEST_ASSERT_EQUAL_INT(static_cast<int>(target), static_cast<int>(current + 0.5f));
  }
}

void test_pauses_and_zero_elapsed_do_not_snap_to_target()
{
  TEST_ASSERT_EQUAL_FLOAT(15.0f, brightness::smoothAutoBrightness(15.0f, 255.0f, 0));
  TEST_ASSERT_EQUAL_FLOAT(128.0f, brightness::smoothAutoBrightness(128.0f, 128.0f, 20));
  const float resumed = brightness::smoothAutoBrightness(15.0f, 255.0f, 60000);
  TEST_ASSERT_TRUE(resumed > 15.0f && resumed < 45.0f);
}

void test_target_reversal_fades_from_current_brightness()
{
  const float current = transition(15.0f, 255.0f, 500, 20);
  const float next = brightness::smoothAutoBrightness(current, 15.0f, 20);
  TEST_ASSERT_TRUE(next < current && next > current - 6.0f);
}

int main()
{
  UNITY_BEGIN();
  RUN_TEST(test_new_lux_target_does_not_cause_a_brightness_jump);
  RUN_TEST(test_transition_speed_is_independent_of_loop_cadence);
  RUN_TEST(test_fades_are_monotonic_and_reach_both_integer_endpoints);
  RUN_TEST(test_pauses_and_zero_elapsed_do_not_snap_to_target);
  RUN_TEST(test_target_reversal_fades_from_current_brightness);
  return UNITY_END();
}
