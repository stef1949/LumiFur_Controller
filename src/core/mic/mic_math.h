#ifndef MIC_MATH_H
#define MIC_MATH_H

#include <cstdint>

float micComputeSignalAboveAmbient(float smoothedSignal, float ambientNoise);
float micComputeOpenThreshold(float ambientNoise);
float micComputeCloseThreshold(float ambientNoise);
float micComputeAmbientUpdateLimit(float ambientNoise);
float micComputeImpulseAvgLimit(float ambientNoise);
float micComputePeakToAvg(std::uint32_t peakAbs, float currentAvgAbsSignal);
bool micIsImpulse(std::uint32_t peakAbs, float currentAvgAbsSignal, float ambientNoise);
float micComputeBrightnessTarget(float smoothedSignal, float ambientNoise);
float micUpdateBrightnessEma(float brightnessEma, float targetBrightness);

#endif // MIC_MATH_H
