#include <unity.h>

#include "core/MotionState.h"
using namespace motion;



void setUp() {}

void tearDown() {}

void test_production_thresholds()
{
  TEST_ASSERT_EQUAL_FLOAT(1.0f,threshold(true)); TEST_ASSERT_EQUAL_FLOAT(1.0f,threshold(false));
}

void test_first_sample_establishes_baseline()
{
  Sample s; s.update(5,6,7); TEST_ASSERT_TRUE(s.valid); TEST_ASSERT_FALSE(s.shake); TEST_ASSERT_FALSE(s.activity);
}

void test_motion_detected_on_each_axis()
{
  for(int axis=0;axis<3;++axis){Sample s;s.update(0,0,0);s.update(axis==0?2:0,axis==1?2:0,axis==2?2:0);TEST_ASSERT_TRUE(s.shake);TEST_ASSERT_TRUE(s.activity);}
}

void test_negative_motion_is_detected()
{
  Sample s;s.update(0,0,0);s.update(-2,-3,-4);TEST_ASSERT_TRUE(s.shake);TEST_ASSERT_EQUAL_FLOAT(3,s.deltaY);
}

void test_exact_threshold_is_not_motion()
{
  Sample s;s.update(0,0,0);s.update(1,1,1);TEST_ASSERT_FALSE(s.shake);
}

void test_stable_sample_clears_motion()
{
  Sample s;s.update(0,0,0);s.update(3,0,0);s.update(3,0,0);TEST_ASSERT_FALSE(s.activity);TEST_ASSERT_EQUAL_FLOAT(0,s.deltaX);
}

void test_small_movements_stay_below_threshold()
{
  Sample s;s.update(0,0,0);s.update(.2f,.3f,.4f);TEST_ASSERT_FALSE(s.shake);
}

void test_delta_is_absolute()
{
  TEST_ASSERT_EQUAL_FLOAT(3,delta(2,5));TEST_ASSERT_EQUAL_FLOAT(3,delta(5,2));
}

int main()
{
  UNITY_BEGIN();

  RUN_TEST(test_production_thresholds);

  RUN_TEST(test_first_sample_establishes_baseline);

  RUN_TEST(test_motion_detected_on_each_axis);

  RUN_TEST(test_negative_motion_is_detected);

  RUN_TEST(test_exact_threshold_is_not_motion);

  RUN_TEST(test_stable_sample_clears_motion);

  RUN_TEST(test_small_movements_stay_below_threshold);

  RUN_TEST(test_delta_is_absolute);

  return UNITY_END();
}
