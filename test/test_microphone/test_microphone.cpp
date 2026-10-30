#include <unity.h>

#include "core/mic/mic_math.h"
#include "core/mic/mic_config.h"



void setUp() {}

void tearDown() {}

void test_silence_keeps_mouth_closed()
{
  float noise=MIC_NOISE_FLOOR_INIT,peak=MIC_PEAK_REF_INIT,envelope=0;for(int i=0;i<100;++i){noise=micUpdateNoiseFloor(noise,500);float speech=micComputeSpeechLevel(500,noise);peak=micUpdatePeakReference(peak,speech);envelope=micApplyAttackReleaseEma(envelope,micNormalizeSpeechLevel(speech,peak),MIC_SIGNAL_ATTACK_ALPHA,MIC_SIGNAL_RELEASE_ALPHA);}TEST_ASSERT_EQUAL_FLOAT(0,micComputeMouthOpennessTarget(envelope));
}

void test_speech_opens_then_silence_closes_mouth()
{
  float peak=MIC_PEAK_REF_INIT,envelope=0;for(int i=0;i<20;++i){float speech=micComputeSpeechLevel(9000,MIC_NOISE_FLOOR_INIT);peak=micUpdatePeakReference(peak,speech);envelope=micApplyAttackReleaseEma(envelope,micNormalizeSpeechLevel(speech,peak),MIC_SIGNAL_ATTACK_ALPHA,MIC_SIGNAL_RELEASE_ALPHA);}TEST_ASSERT_GREATER_THAN_FLOAT(.5f,micComputeMouthOpennessTarget(envelope));for(int i=0;i<100;++i)envelope=micApplyAttackReleaseEma(envelope,0,MIC_SIGNAL_ATTACK_ALPHA,MIC_SIGNAL_RELEASE_ALPHA);TEST_ASSERT_EQUAL_FLOAT(0,micComputeMouthOpennessTarget(envelope));
}

void test_pipeline_brightness_stays_in_bounds()
{
  for(int i=0;i<=100;++i){float brightness=micComputeBrightnessTarget(i/100.0f);TEST_ASSERT_GREATER_OR_EQUAL_FLOAT(MIC_MIN_BRIGHTNESS,brightness);TEST_ASSERT_LESS_OR_EQUAL_FLOAT(MIC_MAX_BRIGHTNESS,brightness);}
}

int main()
{
  UNITY_BEGIN();

  RUN_TEST(test_silence_keeps_mouth_closed);

  RUN_TEST(test_speech_opens_then_silence_closes_mouth);

  RUN_TEST(test_pipeline_brightness_stays_in_bounds);

  return UNITY_END();
}
