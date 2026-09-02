#include "core/mouth/MouthMorph.h"

#include <cstring>

namespace mouth
{
namespace
{

constexpr std::uint8_t kBayer4x4[16] = {
    0, 8, 2, 10,
    12, 4, 14, 6,
    3, 11, 1, 9,
    15, 7, 13, 5};

bool getXbmPixel(const std::uint8_t *xbm, int byteWidth, int x, int y)
{
  const std::uint8_t rowByte = xbm[y * byteWidth + (x >> 3)];
  return (rowByte & static_cast<std::uint8_t>(0x80U >> (x & 7))) != 0;
}

void setXbmPixel(std::uint8_t *xbm, int byteWidth, int x, int y)
{
  xbm[y * byteWidth + (x >> 3)] |= static_cast<std::uint8_t>(0x80U >> (x & 7));
}

} // namespace

std::uint8_t quantizeOpenness(std::uint8_t openness)
{
  return static_cast<std::uint8_t>(
      (static_cast<std::uint16_t>(openness) * (kFrameCount - 1U) + 127U) / 255U);
}

std::uint8_t opennessForFrame(std::uint8_t frameIndex)
{
  if (frameIndex >= kFrameCount)
  {
    frameIndex = kFrameCount - 1U;
  }
  return static_cast<std::uint8_t>(
      (static_cast<std::uint16_t>(frameIndex) * 255U + ((kFrameCount - 1U) / 2U)) /
      (kFrameCount - 1U));
}

int warpedSourceRow(int outputRow, int height, int collapsedHeight, std::uint8_t openness)
{
  if (height <= 0 || collapsedHeight <= 0 || collapsedHeight > height ||
      outputRow < 0 || outputRow >= height)
  {
    return -1;
  }

  const int expansion = height - collapsedHeight;
  const int scaledHeight = collapsedHeight +
                           ((expansion * static_cast<int>(openness) + 127) / 255);
  const int topRow = height - scaledHeight;
  if (outputRow < topRow)
  {
    return -1;
  }
  if (scaledHeight == 1)
  {
    return height - 1;
  }

  return ((outputRow - topRow) * (height - 1) + ((scaledHeight - 1) / 2)) /
         (scaledHeight - 1);
}

bool buildInterpolatedXbm(const std::uint8_t *closedXbm,
                          const std::uint8_t *openXbm,
                          int width,
                          int height,
                          int collapsedHeight,
                          std::uint8_t openness,
                          bool mirrorDither,
                          std::uint8_t *outputXbm,
                          std::size_t outputBytes)
{
  if (closedXbm == nullptr || openXbm == nullptr || outputXbm == nullptr ||
      width <= 0 || height <= 0 || collapsedHeight <= 0 || collapsedHeight > height)
  {
    return false;
  }

  const int byteWidth = (width + 7) >> 3;
  const std::size_t requiredBytes = static_cast<std::size_t>(byteWidth) *
                                    static_cast<std::size_t>(height);
  if (outputBytes < requiredBytes)
  {
    return false;
  }

  if (openness == 0U)
  {
    std::memcpy(outputXbm, closedXbm, requiredBytes);
    return true;
  }
  if (openness == 255U)
  {
    std::memcpy(outputXbm, openXbm, requiredBytes);
    return true;
  }

  std::memset(outputXbm, 0, requiredBytes);
  const int closedWeight = 255 - static_cast<int>(openness);
  const int openWeight = static_cast<int>(openness);

  for (int y = 0; y < height; ++y)
  {
    const int openSourceY = warpedSourceRow(y, height, collapsedHeight, openness);
    for (int x = 0; x < width; ++x)
    {
      const bool closedOn = getXbmPixel(closedXbm, byteWidth, x, y);
      const bool openOn = openSourceY >= 0 &&
                          getXbmPixel(openXbm, byteWidth, x, openSourceY);
      const int coverage = (closedOn ? closedWeight : 0) +
                           (openOn ? openWeight : 0);
      if (coverage == 0)
      {
        continue;
      }

      const int ditherX = mirrorDither ? (width - 1 - x) : x;
      const std::uint8_t threshold = static_cast<std::uint8_t>(
          kBayer4x4[((y & 3) << 2) | (ditherX & 3)] * 16U + 8U);
      if (coverage >= threshold)
      {
        setXbmPixel(outputXbm, byteWidth, x, y);
      }
    }
  }

  return true;
}

} // namespace mouth
