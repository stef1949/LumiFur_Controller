#include <unity.h>
#include "ble/LuxNotifyPolicy.h"

using lux::shouldNotifyLux;

void setUp() {}
void tearDown() {}

void test_small_values_notify_on_any_change()
{
  TEST_ASSERT_TRUE(shouldNotifyLux(1, 0));
  TEST_ASSERT_TRUE(shouldNotifyLux(0, 1));
  TEST_ASSERT_TRUE(shouldNotifyLux(20, 19));
}

void test_equal_values_do_not_notify()
{
  TEST_ASSERT_FALSE(shouldNotifyLux(0, 0));
  TEST_ASSERT_FALSE(shouldNotifyLux(5, 5));
  TEST_ASSERT_FALSE(shouldNotifyLux(100, 100));
  TEST_ASSERT_FALSE(shouldNotifyLux(65535, 65535));
}

void test_larger_values_need_ten_percent_change()
{
  TEST_ASSERT_FALSE(shouldNotifyLux(109, 100));
  TEST_ASSERT_TRUE(shouldNotifyLux(110, 100));
  TEST_ASSERT_FALSE(shouldNotifyLux(91, 100));
  TEST_ASSERT_TRUE(shouldNotifyLux(90, 100));
  TEST_ASSERT_TRUE(shouldNotifyLux(900, 1000));
  TEST_ASSERT_FALSE(shouldNotifyLux(21, 20));
  TEST_ASSERT_TRUE(shouldNotifyLux(22, 20));
}

void test_uint16_extremes()
{
  TEST_ASSERT_TRUE(shouldNotifyLux(65535, 0));
  TEST_ASSERT_TRUE(shouldNotifyLux(0, 65535));
  TEST_ASSERT_TRUE(shouldNotifyLux(58982, 65535));
  TEST_ASSERT_FALSE(shouldNotifyLux(58983, 65535));
  TEST_ASSERT_TRUE(shouldNotifyLux(65535, 59577));
  TEST_ASSERT_FALSE(shouldNotifyLux(65535, 59580));
}

int main()
{
  UNITY_BEGIN();
  RUN_TEST(test_small_values_notify_on_any_change);
  RUN_TEST(test_equal_values_do_not_notify);
  RUN_TEST(test_larger_values_need_ten_percent_change);
  RUN_TEST(test_uint16_extremes);
  return UNITY_END();
}
