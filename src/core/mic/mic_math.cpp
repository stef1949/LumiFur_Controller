#include "core/mic/mic_math.h"

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

float micComputeSignalAboveAmbient(float smoothedSignal, float ambientNoise)
{
  float signal = smoothedSignal - ambientNoise;
  return (signal < 0.0f) ? 0.0f : signal;
}

float micComputeOpenThreshold(float ambientNoise)
{
  return ambientNoise + MIC_SENSITIVITY_OFFSET_ABOVE_AMBIENT;
}

float micComputeCloseThreshold(float ambientNoise)
{
  return ambientNoise + MIC_SENSITIVITY_OFFSET_ABOVE_AMBIENT * MIC_CLOSE_HYSTERESIS_RATIO;
}

float micComputeAmbientUpdateLimit(float ambientNoise)
{
  return ambientNoise + MIC_SENSITIVITY_OFFSET_ABOVE_AMBIENT * MIC_AMBIENT_UPDATE_BAND;
}

float micComputeImpulseAvgLimit(float ambientNoise)
{
  return ambientNoise + MIC_SENSITIVITY_OFFSET_ABOVE_AMBIENT * MIC_IMPULSE_MAX_AVG_RATIO;
}

float micComputePeakToAvg(std::uint32_t peakAbs, float currentAvgAbsSignal)
{
  if (currentAvgAbsSignal > 1.0f)
  {
    return static_cast<float>(peakAbs) / currentAvgAbsSignal;
  }
  return 0.0f;
}

bool micIsImpulse(std::uint32_t peakAbs, float currentAvgAbsSignal, float ambientNoise)
{
  const float impulseAvgLimit = micComputeImpulseAvgLimit(ambientNoise);
  const float peakToAvg = micComputePeakToAvg(peakAbs, currentAvgAbsSignal);
  return (peakToAvg >= MIC_IMPULSE_PEAK_RATIO) && (currentAvgAbsSignal < impulseAvgLimit);
}

float micComputeBrightnessTarget(float smoothedSignal, float ambientNoise)
{
  const float signalAboveAmbient = micComputeSignalAboveAmbient(smoothedSignal, ambientNoise);
  const float brightnessMappingRange = MIC_SENSITIVITY_OFFSET_ABOVE_AMBIENT * 2.5f;
  float normalized = 0.0f;
  if (brightnessMappingRange > 0.0f)
  {
    normalized = signalAboveAmbient / brightnessMappingRange;
  }
  if (normalized > 1.0f)
  {
    normalized = 1.0f;
  }
  return static_cast<float>(MIC_MIN_BRIGHTNESS) +
         (static_cast<float>(MIC_MAX_BRIGHTNESS - MIC_MIN_BRIGHTNESS) * normalized);
}

float micUpdateBrightnessEma(float brightnessEma, float targetBrightness)
{
  const float alpha = (targetBrightness > brightnessEma) ? MIC_BRIGHTNESS_ATTACK_ALPHA : MIC_BRIGHTNESS_RELEASE_ALPHA;
  brightnessEma += alpha * (targetBrightness - brightnessEma);
  if (brightnessEma < static_cast<float>(MIC_MIN_BRIGHTNESS))
  {
    brightnessEma = static_cast<float>(MIC_MIN_BRIGHTNESS);
  }
  if (brightnessEma > static_cast<float>(MIC_MAX_BRIGHTNESS))
  {
    brightnessEma = static_cast<float>(MIC_MAX_BRIGHTNESS);
  }
  return brightnessEma;
}
