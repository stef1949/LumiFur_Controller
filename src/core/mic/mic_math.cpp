#include "core/mic/mic_math.h"

#include <cmath>

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

float micClamp(float value, float minValue, float maxValue)
{
  if (value < minValue)
  {
    return minValue;
  }
  if (value > maxValue)
  {
    return maxValue;
  }
  return value;
}

float micClamp01(float value)
{
  return micClamp(value, 0.0f, 1.0f);
}

float micApplyEma(float currentValue, float targetValue, float alpha)
{
  const float clampedAlpha = micClamp01(alpha);
  return currentValue + clampedAlpha * (targetValue - currentValue);
}

float micApplyAttackReleaseEma(float currentValue, float targetValue, float attackAlpha, float releaseAlpha)
{
  const float alpha = (targetValue > currentValue) ? attackAlpha : releaseAlpha;
  return micApplyEma(currentValue, targetValue, alpha);
}

float micUpdateNoiseFloor(float noiseFloor, float blockEnvelope)
{
  if (blockEnvelope > noiseFloor + MIC_NOISE_FLOOR_GATE_BAND)
  {
    return micClamp(noiseFloor, MIC_NOISE_FLOOR_MIN, MIC_NOISE_FLOOR_MAX);
  }

  const float alpha = (blockEnvelope >= noiseFloor) ? MIC_NOISE_FLOOR_RISE_ALPHA : MIC_NOISE_FLOOR_FALL_ALPHA;
  const float updated = micApplyEma(noiseFloor, blockEnvelope, alpha);
  return micClamp(updated, MIC_NOISE_FLOOR_MIN, MIC_NOISE_FLOOR_MAX);
}

float micComputeSpeechLevel(float blockEnvelope, float noiseFloor)
{
  const float speech = blockEnvelope - (noiseFloor + MIC_NOISE_GATE_MARGIN);
  return (speech > 0.0f) ? speech : 0.0f;
}

float micUpdatePeakReference(float peakReference, float speechLevel)
{
  const float target = (speechLevel > MIC_PEAK_REF_MIN) ? speechLevel : MIC_PEAK_REF_MIN;
  const float updated = micApplyAttackReleaseEma(
      peakReference,
      target,
      MIC_PEAK_REF_ATTACK_ALPHA,
      MIC_PEAK_REF_RELEASE_ALPHA);
  return micClamp(updated, MIC_PEAK_REF_MIN, MIC_PEAK_REF_MAX);
}

float micNormalizeSpeechLevel(float speechLevel, float peakReference)
{
  if (peakReference <= 0.0f)
  {
    return 0.0f;
  }
  return micClamp01(speechLevel / peakReference);
}

float micComputeBrightnessTarget(float normalizedEnvelope)
{
  const float shapedEnvelope = std::sqrt(micClamp01(normalizedEnvelope));
  return static_cast<float>(MIC_MIN_BRIGHTNESS) +
         (static_cast<float>(MIC_MAX_BRIGHTNESS - MIC_MIN_BRIGHTNESS) * shapedEnvelope);
}

float micComputeMouthOpennessTarget(float normalizedEnvelope)
{
  const float thresholdRange = MIC_MOUTH_FULL_OPEN_THRESHOLD - MIC_MOUTH_CLOSE_THRESHOLD;
  if (thresholdRange <= 0.0f)
  {
    return normalizedEnvelope >= MIC_MOUTH_FULL_OPEN_THRESHOLD ? 1.0f : 0.0f;
  }

  const float linear = micClamp01(
      (normalizedEnvelope - MIC_MOUTH_CLOSE_THRESHOLD) / thresholdRange);
  return linear * linear * (3.0f - 2.0f * linear);
}

bool micShouldOpenMouth(float normalizedEnvelope, bool mouthOpen)
{
  if (mouthOpen)
  {
    return normalizedEnvelope >= MIC_MOUTH_CLOSE_THRESHOLD;
  }
  return normalizedEnvelope >= MIC_MOUTH_OPEN_THRESHOLD;
}

bool micShouldApplyPanelHeadroom(bool overrideEnabled, bool micFaceActive)
{
  return overrideEnabled && micFaceActive;
}

std::uint8_t micResolvePanelBrightness(std::uint8_t requestedBrightness, bool panelHeadroomEnabled)
{
  return panelHeadroomEnabled ? 255U : requestedBrightness;
}

std::uint8_t micComputeMouthOverrideBrightness(std::uint8_t microphoneBrightness,
                                               std::uint8_t displayBrightnessFloor)
{
  constexpr std::uint16_t kOutputMaximum = 255U;
  constexpr std::uint16_t kMicrophoneMinimum = MIC_MIN_BRIGHTNESS;
  constexpr std::uint16_t kMicrophoneMaximum = MIC_MAX_BRIGHTNESS;

  if (microphoneBrightness <= kMicrophoneMinimum ||
      kMicrophoneMaximum <= kMicrophoneMinimum)
  {
    return displayBrightnessFloor;
  }
  if (microphoneBrightness >= kMicrophoneMaximum)
  {
    return static_cast<std::uint8_t>(kOutputMaximum);
  }

  const std::uint16_t microphoneRange =
      kMicrophoneMaximum - kMicrophoneMinimum;
  const std::uint16_t microphoneLevel =
      static_cast<std::uint16_t>(microphoneBrightness) - kMicrophoneMinimum;
  const std::uint16_t availableBoost =
      kOutputMaximum - static_cast<std::uint16_t>(displayBrightnessFloor);
  const std::uint32_t roundedBoost =
      static_cast<std::uint32_t>(availableBoost) * microphoneLevel +
      (microphoneRange / 2U);

  return static_cast<std::uint8_t>(
      static_cast<std::uint16_t>(displayBrightnessFloor) +
      static_cast<std::uint16_t>(roundedBoost / microphoneRange));
}

std::uint16_t micBrightnessToFixedScale(std::uint8_t brightness)
{
  return static_cast<std::uint16_t>(
      (static_cast<std::uint32_t>(brightness) * 256U + 127U) / 255U);
}
