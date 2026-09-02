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

void test_clamp_helpers(void)
{
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, micClamp(-1.0f, 0.0f, 1.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.5f, micClamp(0.5f, 0.0f, 1.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, micClamp(2.0f, 0.0f, 1.0f));

  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, micClamp01(-0.5f));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.25f, micClamp01(0.25f));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, micClamp01(1.5f));
}

void test_ema_helpers(void)
{
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 12.0f, micApplyEma(10.0f, 20.0f, 0.2f));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 14.5f, micApplyAttackReleaseEma(10.0f, 20.0f, 0.45f, 0.12f));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 18.8f, micApplyAttackReleaseEma(20.0f, 10.0f, 0.45f, 0.12f));
}

void test_noise_floor_tracks_quiet_audio(void)
{
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 501.0f, micUpdateNoiseFloor(500.0f, 600.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 499.9f, micUpdateNoiseFloor(500.0f, 400.0f));
}

void test_noise_floor_ignores_active_audio(void)
{
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 500.0f, micUpdateNoiseFloor(500.0f, 2000.0f));
}

void test_speech_level_and_normalization(void)
{
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, micComputeSpeechLevel(1200.0f, 500.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 200.0f, micComputeSpeechLevel(1500.0f, 500.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.05f, micNormalizeSpeechLevel(200.0f, 4000.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, micNormalizeSpeechLevel(9000.0f, 4000.0f));
}

void test_peak_reference_attack_release_and_clamp(void)
{
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 4120.0f, micUpdatePeakReference(4000.0f, 8000.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 9994.0f, micUpdatePeakReference(10000.0f, 0.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, MIC_PEAK_REF_MAX, micUpdatePeakReference(MIC_PEAK_REF_MAX, MIC_PEAK_REF_MAX * 2.0f));
}

void test_brightness_target_bounds_and_shape(void)
{
  TEST_ASSERT_FLOAT_WITHIN(0.01f, static_cast<float>(MIC_MIN_BRIGHTNESS), micComputeBrightnessTarget(0.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, static_cast<float>(MIC_MAX_BRIGHTNESS), micComputeBrightnessTarget(1.0f));

  const float expectedMid = static_cast<float>(MIC_MIN_BRIGHTNESS) +
                            (static_cast<float>(MIC_MAX_BRIGHTNESS - MIC_MIN_BRIGHTNESS) * 0.5f);
  TEST_ASSERT_FLOAT_WITHIN(0.05f, expectedMid, micComputeBrightnessTarget(0.25f));
}

void test_mouth_hysteresis(void)
{
  TEST_ASSERT_TRUE(micShouldOpenMouth(MIC_MOUTH_OPEN_THRESHOLD, false));
  TEST_ASSERT_FALSE(micShouldOpenMouth(MIC_MOUTH_OPEN_THRESHOLD - 0.01f, false));
  TEST_ASSERT_TRUE(micShouldOpenMouth(MIC_MOUTH_CLOSE_THRESHOLD + 0.01f, true));
  TEST_ASSERT_FALSE(micShouldOpenMouth(MIC_MOUTH_CLOSE_THRESHOLD - 0.01f, true));
}

void test_continuous_mouth_openness_curve(void)
{
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f,
                           micComputeMouthOpennessTarget(MIC_MOUTH_CLOSE_THRESHOLD));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f,
                           micComputeMouthOpennessTarget(MIC_MOUTH_FULL_OPEN_THRESHOLD));

  const float midpoint = (MIC_MOUTH_CLOSE_THRESHOLD + MIC_MOUTH_FULL_OPEN_THRESHOLD) * 0.5f;
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, micComputeMouthOpennessTarget(midpoint));
  TEST_ASSERT_TRUE(micComputeMouthOpennessTarget(MIC_MOUTH_OPEN_THRESHOLD) > 0.0f);
  TEST_ASSERT_TRUE(micComputeMouthOpennessTarget(MIC_MOUTH_OPEN_THRESHOLD) < 0.5f);
}

void test_mouth_brightness_override_requires_option_and_activity(void)
{
  TEST_ASSERT_FALSE(micShouldOverrideMouthBrightness(false, false));
  TEST_ASSERT_FALSE(micShouldOverrideMouthBrightness(false, true));
  TEST_ASSERT_FALSE(micShouldOverrideMouthBrightness(true, false));
  TEST_ASSERT_TRUE(micShouldOverrideMouthBrightness(true, true));
}

void test_panel_headroom_depends_on_option_and_mic_face_not_mouth_activity(void)
{
  TEST_ASSERT_FALSE(micShouldApplyPanelHeadroom(false, false));
  TEST_ASSERT_FALSE(micShouldApplyPanelHeadroom(false, true));
  TEST_ASSERT_FALSE(micShouldApplyPanelHeadroom(true, false));
  TEST_ASSERT_TRUE(micShouldApplyPanelHeadroom(true, true));

  // Headroom is already maximum while the mouth is idle, so later activity
  // cannot be capped by a user or auto-brightness hardware setting.
  TEST_ASSERT_EQUAL_UINT8(255, micResolvePanelBrightness(32, true));
  TEST_ASSERT_FALSE(micShouldOverrideMouthBrightness(true, false));
}

void test_mouth_override_resolves_hardware_brightness(void)
{
  TEST_ASSERT_EQUAL_UINT8(32, micResolvePanelBrightness(32, false));
  TEST_ASSERT_EQUAL_UINT8(255, micResolvePanelBrightness(32, true));
  TEST_ASSERT_EQUAL_UINT8(255, micResolvePanelBrightness(255, true));
}

void setup()
{
  UNITY_BEGIN();
  RUN_TEST(test_clamp_helpers);
  RUN_TEST(test_ema_helpers);
  RUN_TEST(test_noise_floor_tracks_quiet_audio);
  RUN_TEST(test_noise_floor_ignores_active_audio);
  RUN_TEST(test_speech_level_and_normalization);
  RUN_TEST(test_peak_reference_attack_release_and_clamp);
  RUN_TEST(test_brightness_target_bounds_and_shape);
  RUN_TEST(test_mouth_hysteresis);
  RUN_TEST(test_continuous_mouth_openness_curve);
  RUN_TEST(test_mouth_brightness_override_requires_option_and_activity);
  RUN_TEST(test_panel_headroom_depends_on_option_and_mic_face_not_mouth_activity);
  RUN_TEST(test_mouth_override_resolves_hardware_brightness);
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
