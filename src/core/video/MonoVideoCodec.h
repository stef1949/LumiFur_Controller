#pragma once

#include <cstddef>
#include <cstdint>

namespace video
{

static constexpr uint8_t kMonoVideoMagic[4] = {'L', 'F', 'V', '1'};

enum class FrameEncoding : uint8_t
{
  KeyframeRaw = 0,
  DeltaRuns = 1,
};

#pragma pack(push, 1)
struct MonoVideoHeader
{
  uint8_t magic[4];
  uint16_t width;
  uint16_t height;
  uint16_t bytesPerFrame;
  uint16_t reserved;
  uint32_t frameCount;
  uint32_t frameIntervalMicros;
};

struct MonoVideoFrameHeader
{
  uint8_t encoding;
  uint8_t reserved;
  uint16_t payloadSize;
};

struct DeltaRunHeader
{
  uint16_t offset;
  uint16_t length;
};
#pragma pack(pop)

constexpr size_t bytesPerFrameForDimensions(uint16_t width, uint16_t height)
{
  return (static_cast<size_t>(width) * static_cast<size_t>(height)) / 8U;
}

bool isSupportedEncoding(uint8_t encoding);
bool isValidHeader(const MonoVideoHeader &header, uint16_t expectedWidth = 0, uint16_t expectedHeight = 0);
bool applyFramePayload(FrameEncoding encoding,
                       const uint8_t *payload,
                       size_t payloadSize,
                       uint8_t *frameBuffer,
                       size_t frameBufferSize);
bool applyFramePayload(uint8_t encoding,
                       const uint8_t *payload,
                       size_t payloadSize,
                       uint8_t *frameBuffer,
                       size_t frameBufferSize);

} // namespace video
