#include <unity.h>

void setUp(void)
{
}

void tearDown(void)
{
}

void test_mic_placeholder(void)
{
  TEST_IGNORE_MESSAGE("Python mic test is not executed in Unity.");
}

int main()
{
  UNITY_BEGIN();
  RUN_TEST(test_mic_placeholder);
  return UNITY_END();
}
