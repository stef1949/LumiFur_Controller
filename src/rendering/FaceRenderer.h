#pragma once
#include <cstdint>

void prepareInterpolatedMouthFrames(std::uint8_t rawOpenness);
void drawInterpolatedMouthPlasma(std::uint8_t rightTimeOffset,
                                 std::uint8_t leftTimeOffset,
                                 float scale,
                                 float animSpeed,
                                 std::uint8_t brightness);
void drawPlasmaFace();
void updatePlasmaFace();
void updateRotatingSpiral();
void blinkingEyes();
void drawBlush();
void baseFace();
void renderFaceWithPlasma();
void renderSpiralEyesView();
void renderPlasmaFaceView();
void renderMatrixFaceView();
