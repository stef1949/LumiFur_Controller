#include <unity.h>
#include "core/ScrollState.h"

using namespace scroll;

void setUp(void)
{
  // Set up before each test
}

void tearDown(void)
{
  // Clean up after each test
}

void test_scroll_state_initialization(void)
{
  State &s = state();
  
  // Test initial values
  TEST_ASSERT_EQUAL_UINT8(4, s.speedSetting);
  // At speed=4, interval should be 15 + ((4-1) * 235 / 499) = 16.
  TEST_ASSERT_EQUAL_UINT16(16, s.textIntervalMs);
  TEST_ASSERT_EQUAL_UINT32(0, s.lastScrollTickMs);
  TEST_ASSERT_EQUAL_UINT32(0, s.lastBackgroundTickMs);
  TEST_ASSERT_EQUAL_UINT8(0, s.backgroundOffset);
  TEST_ASSERT_EQUAL_UINT8(0, s.colorOffset);
  TEST_ASSERT_FALSE(s.textInitialized);
}

void test_scroll_speed_update(void)
{
  State &s = state();
  
  // Change speed and update interval
  s.speedSetting = 50;
  updateIntervalFromSpeed();
  
  // Verify interval changed (should be somewhere between min and max)
  TEST_ASSERT_GREATER_THAN_UINT16(15, s.textIntervalMs);
  TEST_ASSERT_LESS_THAN_UINT16(250, s.textIntervalMs);
}

void test_scroll_reset_timing(void)
{
  State &s = state();
  
  // Set some values
  s.lastScrollTickMs = 1000;
  s.lastBackgroundTickMs = 2000;
  s.backgroundOffset = 50;
  s.colorOffset = 100;
  s.textInitialized = true;
  
  // Reset timing
  resetTiming();
  
  // Verify reset
  TEST_ASSERT_EQUAL_UINT32(0, s.lastScrollTickMs);
  TEST_ASSERT_EQUAL_UINT32(0, s.lastBackgroundTickMs);
  TEST_ASSERT_EQUAL_UINT8(0, s.backgroundOffset);
  TEST_ASSERT_EQUAL_UINT8(0, s.colorOffset);
  TEST_ASSERT_FALSE(s.textInitialized);
}

void test_scroll_speed_boundary_slow(void)
{
  State &s = state();
  
  // Test minimum speed (slowest scroll = fastest interval)
  // Note: Lower speed number = faster scrolling in this implementation
  s.speedSetting = 1;
  updateIntervalFromSpeed();
  
  // At speed=1, interval = 15 + 0 = 15 (fastest scrolling)
  TEST_ASSERT_EQUAL_UINT16(15, s.textIntervalMs);
}

void test_scroll_speed_boundary_fast(void)
{
  State &s = state();
  
  // Test maximum speed (slowest scroll = longest interval)
  // Note: Higher speed number = slower scrolling in this implementation
  s.speedSetting = 500;
  updateIntervalFromSpeed();
  
  // At speed=500, interval = 15 + 235 = 250 (slowest scrolling)
  TEST_ASSERT_EQUAL_UINT16(250, s.textIntervalMs);
}

void test_scroll_speed_mid_range(void)
{
  State &s = state();
  
  // Test mid-range speed
  s.speedSetting = 50;
  updateIntervalFromSpeed();
  
  // At speed=50, interval = 15 + ((50-1) * 235 / 499) = 38.
  TEST_ASSERT_EQUAL_UINT16(38, s.textIntervalMs);
}

void setup()
{
  UNITY_BEGIN();
  
  RUN_TEST(test_scroll_state_initialization);
  RUN_TEST(test_scroll_speed_update);
  RUN_TEST(test_scroll_reset_timing);
  RUN_TEST(test_scroll_speed_boundary_slow);
  RUN_TEST(test_scroll_speed_boundary_fast);
  RUN_TEST(test_scroll_speed_mid_range);
  
  UNITY_END();
}

int main()
{
  setup();
  return 0;
}

void loop()
{
  // Unity tests run once
}

