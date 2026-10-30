#include <unity.h>
#include "core/DisplayTiming.h"

void setUp() {}
void tearDown() {}

void test_normal_frame_keeps_phase()
{
  uint32_t wake = 100;
  TEST_ASSERT_EQUAL_UINT32(7, display_timing::nextFrameDelay(103, wake, 10));
  TEST_ASSERT_EQUAL_UINT32(110, wake);
}

void test_slow_frame_skips_missed_deadlines()
{
  uint32_t wake = 0;
  TEST_ASSERT_EQUAL_UINT32(1, display_timing::nextFrameDelay(35, wake, 10));
  TEST_ASSERT_EQUAL_UINT32(35, wake);
  TEST_ASSERT_EQUAL_UINT32(7, display_timing::nextFrameDelay(38, wake, 10));
  TEST_ASSERT_EQUAL_UINT32(45, wake);
}

void test_exact_deadline_yields()
{
  uint32_t wake = 100;
  TEST_ASSERT_EQUAL_UINT32(1, display_timing::nextFrameDelay(110, wake, 10));
  TEST_ASSERT_EQUAL_UINT32(110, wake);
}

void test_tick_rollover()
{
  uint32_t wake = UINT32_MAX - 4;
  TEST_ASSERT_EQUAL_UINT32(3, display_timing::nextFrameDelay(2, wake, 10));
  TEST_ASSERT_EQUAL_UINT32(5, wake);
}

void test_overrun_across_rollover()
{
  uint32_t wake = UINT32_MAX - 4;
  TEST_ASSERT_EQUAL_UINT32(1, display_timing::nextFrameDelay(20, wake, 10));
  TEST_ASSERT_EQUAL_UINT32(20, wake);
}

void test_zero_period_still_yields()
{
  uint32_t wake = 10;
  TEST_ASSERT_EQUAL_UINT32(1, display_timing::nextFrameDelay(10, wake, 0));
}

int main()
{
  UNITY_BEGIN();
  RUN_TEST(test_normal_frame_keeps_phase);
  RUN_TEST(test_slow_frame_skips_missed_deadlines);
  RUN_TEST(test_exact_deadline_yields);
  RUN_TEST(test_tick_rollover);
  RUN_TEST(test_overrun_across_rollover);
  RUN_TEST(test_zero_period_still_yields);
  return UNITY_END();
}
