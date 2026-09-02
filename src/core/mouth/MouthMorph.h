#pragma once

#include <cstddef>
#include <cstdint>

namespace mouth
{

constexpr std::uint8_t kFrameCount = 16;

// Convert the continuously varying microphone value to one of the rendered
// mouth frames, then back to the exact 0-255 interpolation position.
std::uint8_t quantizeOpenness(std::uint8_t openness);
std::uint8_t opennessForFrame(std::uint8_t frameIndex);

// Map a destination row into the fully-open bitmap while keeping the bottom
// of the mouth anchored. Returns -1 for rows above the warped bitmap.
int warpedSourceRow(int outputRow, int height, int collapsedHeight, std::uint8_t openness);

// Build one monochrome XBM frame from the closed and open endpoint bitmaps.
// The open bitmap is expanded vertically and the endpoint masks are blended
// with ordered dithering, giving useful intermediate frames on a 1-bit mask.
bool buildInterpolatedXbm(const std::uint8_t *closedXbm,
                          const std::uint8_t *openXbm,
                          int width,
                          int height,
                          int collapsedHeight,
                          std::uint8_t openness,
                          bool mirrorDither,
                          std::uint8_t *outputXbm,
                          std::size_t outputBytes);

} // namespace mouth
