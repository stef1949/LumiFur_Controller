#include <unity.h>

#ifndef I2S_NUM_1
#define I2S_NUM_1 1
#endif

#ifndef I2S_BITS_PER_SAMPLE_32BIT
#define I2S_BITS_PER_SAMPLE_32BIT 32
#endif

#ifndef I2S_CHANNEL_FMT_RIGHT_LEFT
#define I2S_CHANNEL_FMT_RIGHT_LEFT 0
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

void test_slot_selection_configuration(void)
{
  TEST_ASSERT_EQUAL_INT(I2S_CHANNEL_FMT_RIGHT_LEFT, MIC_CHANNEL_FORMAT);
  TEST_ASSERT_TRUE((MIC_ACTIVE_SLOT_INDEX == 0) || (MIC_ACTIVE_SLOT_INDEX == 1));
}

void test_mouth_threshold_relationships(void)
{
  TEST_ASSERT_TRUE(MIC_MOUTH_OPEN_THRESHOLD > MIC_MOUTH_CLOSE_THRESHOLD);
  TEST_ASSERT_TRUE(micShouldOpenMouth(MIC_MOUTH_OPEN_THRESHOLD, false));
  TEST_ASSERT_FALSE(micShouldOpenMouth(MIC_MOUTH_CLOSE_THRESHOLD - 0.01f, true));
}

void test_speech_gate_requires_margin_above_floor(void)
{
  const float floor = 500.0f;
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, micComputeSpeechLevel(floor + MIC_NOISE_GATE_MARGIN, floor));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 50.0f, micComputeSpeechLevel(floor + MIC_NOISE_GATE_MARGIN + 50.0f, floor));
}

void test_peak_reference_returns_to_minimum(void)
{
  const float released = micUpdatePeakReference(MIC_PEAK_REF_MIN + 1000.0f, 0.0f);
  TEST_ASSERT_TRUE(released < (MIC_PEAK_REF_MIN + 1000.0f));
  TEST_ASSERT_TRUE(released > MIC_PEAK_REF_MIN);
}

void setup()
{
  UNITY_BEGIN();
  RUN_TEST(test_slot_selection_configuration);
  RUN_TEST(test_mouth_threshold_relationships);
  RUN_TEST(test_speech_gate_requires_margin_above_floor);
  RUN_TEST(test_peak_reference_returns_to_minimum);
  UNITY_END();
}

int main()
{
  setup();
  return 0;
}
