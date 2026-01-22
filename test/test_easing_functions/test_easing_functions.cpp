#include <unity.h>

// Local easing implementations for the unit tests.
static float easeInOutQuad(float t)
{
  return t < 0.5f ? 2.0f * t * t : -1.0f + (4.0f - 2.0f * t) * t;
}

static float easeInQuad(float t)
{
  if (t < 0.0f)
    return 0.0f;
  if (t > 1.0f)
    return 1.0f;
  return t * t;
}

static float easeOutQuad(float t)
{
  if (t < 0.0f)
    return 0.0f;
  if (t > 1.0f)
    return 1.0f;
  return 1.0f - (1.0f - t) * (1.0f - t);
}

void setUp(void)
{
  // Set up before each test
}

void tearDown(void)
{
  // Clean up after each test
}

void test_easeInQuad_boundary_conditions(void)
{
  // Test at t=0
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, easeInQuad(0.0f));
  
  // Test at t=1
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.0f, easeInQuad(1.0f));
  
  // Test below 0 (should clamp to 0)
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, easeInQuad(-0.5f));
  
  // Test above 1 (should clamp to 1)
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.0f, easeInQuad(1.5f));
}

void test_easeInQuad_mid_values(void)
{
  // Test at t=0.5 (should be 0.25)
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.25f, easeInQuad(0.5f));
  
  // Test at t=0.25 (should be 0.0625)
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0625f, easeInQuad(0.25f));
  
  // Test at t=0.75 (should be 0.5625)
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.5625f, easeInQuad(0.75f));
}

void test_easeOutQuad_boundary_conditions(void)
{
  // Test at t=0
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, easeOutQuad(0.0f));
  
  // Test at t=1
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.0f, easeOutQuad(1.0f));
  
  // Test below 0 (should clamp to 0)
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, easeOutQuad(-0.5f));
  
  // Test above 1 (should clamp to 1)
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.0f, easeOutQuad(1.5f));
}

void test_easeOutQuad_mid_values(void)
{
  // Test at t=0.5 (should be 0.75)
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.75f, easeOutQuad(0.5f));
  
  // Test at t=0.25 (should be 0.4375)
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.4375f, easeOutQuad(0.25f));
  
  // Test at t=0.75 (should be 0.9375)
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.9375f, easeOutQuad(0.75f));
}

void test_easeInOutQuad_boundary_conditions(void)
{
  // Test at t=0
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, easeInOutQuad(0.0f));
  
  // Test at t=1
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.0f, easeInOutQuad(1.0f));
}

void test_easeInOutQuad_mid_values(void)
{
  // Test at t=0.5 (should be 0.5, the midpoint)
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.5f, easeInOutQuad(0.5f));
  
  // Test at t=0.25 (should be less than 0.25, ease-in effect)
  float result_025 = easeInOutQuad(0.25f);
  TEST_ASSERT_LESS_THAN_FLOAT(0.25f, result_025);  // result < 0.25
  TEST_ASSERT_GREATER_THAN_FLOAT(0.0f, result_025); // result > 0.0
  
  // Test at t=0.75 (should be greater than 0.75, ease-out effect)
  float result_075 = easeInOutQuad(0.75f);
  TEST_ASSERT_GREATER_THAN_FLOAT(0.75f, result_075); // result > 0.75
  TEST_ASSERT_LESS_THAN_FLOAT(1.0f, result_075);     // result < 1.0
}

void test_easing_functions_monotonic(void)
{
  // All easing functions should be monotonically increasing
  float prev_in = 0.0f;
  float prev_out = 0.0f;
  float prev_inout = 0.0f;
  
  for (float t = 0.0f; t <= 1.0f; t += 0.1f)
  {
    float curr_in = easeInQuad(t);
    float curr_out = easeOutQuad(t);
    float curr_inout = easeInOutQuad(t);
    
    TEST_ASSERT_GREATER_OR_EQUAL(prev_in, curr_in);
    TEST_ASSERT_GREATER_OR_EQUAL(prev_out, curr_out);
    TEST_ASSERT_GREATER_OR_EQUAL(prev_inout, curr_inout);
    
    prev_in = curr_in;
    prev_out = curr_out;
    prev_inout = curr_inout;
  }
}

void setup()
{
  UNITY_BEGIN();
  
  RUN_TEST(test_easeInQuad_boundary_conditions);
  RUN_TEST(test_easeInQuad_mid_values);
  RUN_TEST(test_easeOutQuad_boundary_conditions);
  RUN_TEST(test_easeOutQuad_mid_values);
  RUN_TEST(test_easeInOutQuad_boundary_conditions);
  RUN_TEST(test_easeInOutQuad_mid_values);
  RUN_TEST(test_easing_functions_monotonic);
  
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
