#ifndef MIC_MATH_H
#define MIC_MATH_H

#include <cstdint>

float micClamp(float value, float minValue, float maxValue);
float micClamp01(float value);
float micApplyEma(float currentValue, float targetValue, float alpha);
float micApplyAttackReleaseEma(float currentValue, float targetValue, float attackAlpha, float releaseAlpha);
float micUpdateNoiseFloor(float noiseFloor, float blockEnvelope);
float micComputeSpeechLevel(float blockEnvelope, float noiseFloor);
float micUpdatePeakReference(float peakReference, float speechLevel);
float micNormalizeSpeechLevel(float speechLevel, float peakReference);
float micComputeBrightnessTarget(float normalizedEnvelope);
float micComputeMouthOpennessTarget(float normalizedEnvelope);
bool micShouldOpenMouth(float normalizedEnvelope, bool mouthOpen);
bool micShouldApplyPanelHeadroom(bool overrideEnabled, bool micFaceActive);
std::uint8_t micResolvePanelBrightness(std::uint8_t requestedBrightness, bool panelHeadroomEnabled);
std::uint8_t micComputeMouthOverrideBrightness(std::uint8_t microphoneBrightness,
                                               std::uint8_t displayBrightnessFloor);
std::uint16_t micBrightnessToFixedScale(std::uint8_t brightness);

#endif // MIC_MATH_H
