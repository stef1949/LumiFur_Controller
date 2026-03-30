#ifndef MIC_MATH_H
#define MIC_MATH_H

float micClamp(float value, float minValue, float maxValue);
float micClamp01(float value);
float micApplyEma(float currentValue, float targetValue, float alpha);
float micApplyAttackReleaseEma(float currentValue, float targetValue, float attackAlpha, float releaseAlpha);
float micUpdateNoiseFloor(float noiseFloor, float blockEnvelope);
float micComputeSpeechLevel(float blockEnvelope, float noiseFloor);
float micUpdatePeakReference(float peakReference, float speechLevel);
float micNormalizeSpeechLevel(float speechLevel, float peakReference);
float micComputeBrightnessTarget(float normalizedEnvelope);
bool micShouldOpenMouth(float normalizedEnvelope, bool mouthOpen);

#endif // MIC_MATH_H
