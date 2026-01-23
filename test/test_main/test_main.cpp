#include <unity.h>

using TimerHandle_t = void *;

static bool useShakeSensitivity = false;
static constexpr float SLEEP_THRESHOLD = 1.0f;
static constexpr float SHAKE_THRESHOLD = 1.0f;
static volatile unsigned long softMillis = 0;

static float getCurrentThreshold()
{
  return useShakeSensitivity ? SHAKE_THRESHOLD : SLEEP_THRESHOLD;
}

static void onSoftMillisTimer(TimerHandle_t)
{
  softMillis++;
}

void setUp(void)
{
  useShakeSensitivity = false;
  softMillis = 0;
}

void tearDown(void)
{
}

void test_getCurrentThreshold_shake(void)
{
  useShakeSensitivity = true;
  float thr = getCurrentThreshold();
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, SHAKE_THRESHOLD, thr);
}

void test_getCurrentThreshold_sleep(void)
{
  useShakeSensitivity = false;
  float thr = getCurrentThreshold();
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, SLEEP_THRESHOLD, thr);
}

void test_onSoftMillisTimer_increments(void)
{
  unsigned long before = softMillis;
  onSoftMillisTimer(nullptr);
  TEST_ASSERT_EQUAL_UINT(before + 1, softMillis);
}

void setup()
{
  UNITY_BEGIN();
  RUN_TEST(test_getCurrentThreshold_shake);
  RUN_TEST(test_getCurrentThreshold_sleep);
  RUN_TEST(test_onSoftMillisTimer_increments);
  UNITY_END();
}

int main()
{
  setup();
  return 0;
}

void loop()
{
  // no-op
}
