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

void test_signal_above_ambient_clamps(void)
{
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, micComputeSignalAboveAmbient(10.0f, 20.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 5.0f, micComputeSignalAboveAmbient(25.0f, 20.0f));
}

void test_threshold_calculations(void)
{
  const float ambient = 1000.0f;
  TEST_ASSERT_FLOAT_WITHIN(0.01f, ambient + MIC_SENSITIVITY_OFFSET_ABOVE_AMBIENT,
                           micComputeOpenThreshold(ambient));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, ambient + MIC_SENSITIVITY_OFFSET_ABOVE_AMBIENT * MIC_CLOSE_HYSTERESIS_RATIO,
                           micComputeCloseThreshold(ambient));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, ambient + MIC_SENSITIVITY_OFFSET_ABOVE_AMBIENT * MIC_AMBIENT_UPDATE_BAND,
                           micComputeAmbientUpdateLimit(ambient));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, ambient + MIC_SENSITIVITY_OFFSET_ABOVE_AMBIENT * MIC_IMPULSE_MAX_AVG_RATIO,
                           micComputeImpulseAvgLimit(ambient));
}

void test_peak_to_avg_computation(void)
{
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, micComputePeakToAvg(100, 1.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 5.0f, micComputePeakToAvg(500, 100.0f));
}

void test_is_impulse_true(void)
{
  const float ambient = 1000.0f;
  TEST_ASSERT_TRUE(micIsImpulse(1000, 100.0f, ambient));
}

void test_is_impulse_false_low_ratio(void)
{
  const float ambient = 1000.0f;
  TEST_ASSERT_FALSE(micIsImpulse(200, 100.0f, ambient));
}

void test_is_impulse_false_high_average(void)
{
  const float ambient = 1000.0f;
  TEST_ASSERT_FALSE(micIsImpulse(20000, 2000.0f, ambient));
}

void test_brightness_target_bounds(void)
{
  const float ambient = 1000.0f;
  TEST_ASSERT_FLOAT_WITHIN(0.01f, static_cast<float>(MIC_MIN_BRIGHTNESS),
                           micComputeBrightnessTarget(ambient - 10.0f, ambient));

  const float mappingRange = MIC_SENSITIVITY_OFFSET_ABOVE_AMBIENT * 2.5f;
  const float maxSignal = ambient + mappingRange * 2.0f;
  TEST_ASSERT_FLOAT_WITHIN(0.01f, static_cast<float>(MIC_MAX_BRIGHTNESS),
                           micComputeBrightnessTarget(maxSignal, ambient));
}

void test_brightness_target_midpoint(void)
{
  const float ambient = 1000.0f;
  const float mappingRange = MIC_SENSITIVITY_OFFSET_ABOVE_AMBIENT * 2.5f;
  const float midSignal = ambient + (mappingRange * 0.5f);
  const float expected = static_cast<float>(MIC_MIN_BRIGHTNESS) +
                         (static_cast<float>(MIC_MAX_BRIGHTNESS - MIC_MIN_BRIGHTNESS) * 0.5f);
  TEST_ASSERT_FLOAT_WITHIN(0.05f, expected, micComputeBrightnessTarget(midSignal, ambient));
}

void test_brightness_ema_attack_release(void)
{
  const float attack = 100.0f + MIC_BRIGHTNESS_ATTACK_ALPHA * (200.0f - 100.0f);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, attack, micUpdateBrightnessEma(100.0f, 200.0f));

  const float release = 200.0f + MIC_BRIGHTNESS_RELEASE_ALPHA * (100.0f - 200.0f);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, release, micUpdateBrightnessEma(200.0f, 100.0f));
}

void test_brightness_ema_clamps(void)
{
  TEST_ASSERT_FLOAT_WITHIN(0.01f, static_cast<float>(MIC_MAX_BRIGHTNESS),
                           micUpdateBrightnessEma(250.0f, 1000.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, static_cast<float>(MIC_MIN_BRIGHTNESS),
                           micUpdateBrightnessEma(10.0f, 0.0f));
}

void setup()
{
  UNITY_BEGIN();
  RUN_TEST(test_signal_above_ambient_clamps);
  RUN_TEST(test_threshold_calculations);
  RUN_TEST(test_peak_to_avg_computation);
  RUN_TEST(test_is_impulse_true);
  RUN_TEST(test_is_impulse_false_low_ratio);
  RUN_TEST(test_is_impulse_false_high_average);
  RUN_TEST(test_brightness_target_bounds);
  RUN_TEST(test_brightness_target_midpoint);
  RUN_TEST(test_brightness_ema_attack_release);
  RUN_TEST(test_brightness_ema_clamps);
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
