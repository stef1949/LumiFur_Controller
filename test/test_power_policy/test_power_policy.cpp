#include <unity.h>

#include "core/PowerPolicy.h"
using namespace power;



void setUp() {}

void tearDown() {}

void test_inactivity_enters_sleep()
{
  TEST_ASSERT_EQUAL_INT((int)Action::Sleep,(int)evaluate(false,true,false,false,10001,0,10000));
}

void test_timeout_boundary_does_not_sleep()
{
  TEST_ASSERT_EQUAL_INT((int)Action::None,(int)evaluate(false,true,false,false,10000,0,10000));
}

void test_disabling_sleep_wakes_immediately()
{
  TEST_ASSERT_EQUAL_INT((int)Action::Wake,(int)evaluate(true,false,false,false,500,0,10000));
}

void test_activity_wakes()
{
  TEST_ASSERT_EQUAL_INT((int)Action::Wake,(int)evaluate(true,true,true,false,500,0,10000));
}

void test_ota_prevents_sleep_and_wakes()
{
  TEST_ASSERT_EQUAL_INT((int)Action::None,(int)evaluate(false,true,false,true,20000,0,10000)); TEST_ASSERT_EQUAL_INT((int)Action::Wake,(int)evaluate(true,true,false,true,20000,0,10000));
}

void test_disabled_sleep_stays_awake()
{
  TEST_ASSERT_EQUAL_INT((int)Action::None,(int)evaluate(false,false,false,false,20000,0,10000));
}

void test_timeout_across_rollover()
{
  TEST_ASSERT_EQUAL_INT((int)Action::Sleep,(int)evaluate(false,true,false,false,5000,UINT32_MAX-6000,10000));
}

int main()
{
  UNITY_BEGIN();

  RUN_TEST(test_inactivity_enters_sleep);

  RUN_TEST(test_timeout_boundary_does_not_sleep);

  RUN_TEST(test_disabling_sleep_wakes_immediately);

  RUN_TEST(test_activity_wakes);

  RUN_TEST(test_ota_prevents_sleep_and_wakes);

  RUN_TEST(test_disabled_sleep_stays_awake);

  RUN_TEST(test_timeout_across_rollover);

  return UNITY_END();
}
