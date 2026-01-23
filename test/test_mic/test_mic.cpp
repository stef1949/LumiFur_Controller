#include <unity.h>

#ifndef I2S_NUM_1
#define I2S_NUM_1 1
#endif

#ifndef I2S_BITS_PER_SAMPLE_32BIT
#define I2S_BITS_PER_SAMPLE_32BIT 32
#endif

#ifndef I2S_CHANNEL_FMT_ONLY_LEFT
#define I2S_CHANNEL_FMT_ONLY_LEFT 1
#endif

#ifndef I2S_COMM_FORMAT_STAND_I2S
#define I2S_COMM_FORMAT_STAND_I2S 1
#endif

#include "core/mic/mic_config.h"
#include "core/mic/mic_math.h"

void setUp(void)
{
}

void tearDown(void)
{
}

void test_threshold_relationships(void)
{
  const float ambient = 1000.0f;
  const float openThreshold = micComputeOpenThreshold(ambient);
  const float closeThreshold = micComputeCloseThreshold(ambient);
  const float updateLimit = micComputeAmbientUpdateLimit(ambient);
  const float impulseAvgLimit = micComputeImpulseAvgLimit(ambient);

  TEST_ASSERT_GREATER_THAN_FLOAT(closeThreshold, openThreshold);
  TEST_ASSERT_GREATER_THAN_FLOAT(updateLimit, openThreshold);
  TEST_ASSERT_GREATER_THAN_FLOAT(impulseAvgLimit, openThreshold);
  TEST_ASSERT_GREATER_THAN_FLOAT(ambient, closeThreshold);
}

void test_peak_to_avg_zero_when_avg_leq_one(void)
{
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, micComputePeakToAvg(100, 1.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, micComputePeakToAvg(100, 0.5f));
}

void test_is_impulse_at_threshold_true(void)
{
  const float ambient = 1000.0f;
  const float avg = 100.0f;
  const std::uint32_t peak = static_cast<std::uint32_t>(MIC_IMPULSE_PEAK_RATIO * avg);
  TEST_ASSERT_TRUE(micIsImpulse(peak, avg, ambient));
}

void test_is_impulse_at_limit_false(void)
{
  const float ambient = 1000.0f;
  const float avg = micComputeImpulseAvgLimit(ambient);
  const std::uint32_t peak = static_cast<std::uint32_t>(MIC_IMPULSE_PEAK_RATIO * avg);
  TEST_ASSERT_FALSE(micIsImpulse(peak, avg, ambient));
}

void test_brightness_target_clamps_min_max(void)
{
  const float ambient = 1000.0f;
  const float mappingRange = MIC_SENSITIVITY_OFFSET_ABOVE_AMBIENT * 2.5f;
  const float lowSignal = ambient - 100.0f;
  const float highSignal = ambient + (mappingRange * 5.0f);

  TEST_ASSERT_FLOAT_WITHIN(0.01f, static_cast<float>(MIC_MIN_BRIGHTNESS),
                           micComputeBrightnessTarget(lowSignal, ambient));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, static_cast<float>(MIC_MAX_BRIGHTNESS),
                           micComputeBrightnessTarget(highSignal, ambient));
}

void test_brightness_ema_no_change_when_equal(void)
{
  const float value = 123.4f;
  TEST_ASSERT_FLOAT_WITHIN(0.01f, value, micUpdateBrightnessEma(value, value));
}

void setup()
{
  UNITY_BEGIN();
  RUN_TEST(test_threshold_relationships);
  RUN_TEST(test_peak_to_avg_zero_when_avg_leq_one);
  RUN_TEST(test_is_impulse_at_threshold_true);
  RUN_TEST(test_is_impulse_at_limit_false);
  RUN_TEST(test_brightness_target_clamps_min_max);
  RUN_TEST(test_brightness_ema_no_change_when_equal);
  UNITY_END();
}

int main()
{
  setup();
  return 0;
}
