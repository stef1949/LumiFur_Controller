#include "core/video/MonoVideoCodec.h"

#include <cstring>

namespace video
{

bool isSupportedEncoding(uint8_t encoding)
{
  return encoding == static_cast<uint8_t>(FrameEncoding::KeyframeRaw) ||
         encoding == static_cast<uint8_t>(FrameEncoding::DeltaRuns);
}

bool isValidHeader(const MonoVideoHeader &header, uint16_t expectedWidth, uint16_t expectedHeight)
{
  if (std::memcmp(header.magic, kMonoVideoMagic, sizeof(kMonoVideoMagic)) != 0)
  {
    return false;
  }
  if (header.width == 0 || header.height == 0)
  {
    return false;
  }
  if ((header.width % 8U) != 0U)
  {
    return false;
  }
  if (expectedWidth != 0 && header.width != expectedWidth)
  {
    return false;
  }
  if (expectedHeight != 0 && header.height != expectedHeight)
  {
    return false;
  }

  const size_t expectedBytes = bytesPerFrameForDimensions(header.width, header.height);
  if (expectedBytes == 0 || expectedBytes > UINT16_MAX)
  {
    return false;
  }
  if (header.bytesPerFrame != expectedBytes)
  {
    return false;
  }
  if (header.frameCount == 0 || header.frameIntervalMicros == 0)
  {
    return false;
  }
  return true;
}

bool applyFramePayload(FrameEncoding encoding,
                       const uint8_t *payload,
                       size_t payloadSize,
                       uint8_t *frameBuffer,
                       size_t frameBufferSize)
{
  if (!payload || !frameBuffer || frameBufferSize == 0)
  {
    return false;
  }

  switch (encoding)
  {
  case FrameEncoding::KeyframeRaw:
    if (payloadSize != frameBufferSize)
    {
      return false;
    }
    std::memcpy(frameBuffer, payload, frameBufferSize);
    return true;

  case FrameEncoding::DeltaRuns:
  {
    size_t cursor = 0;
    while (cursor < payloadSize)
    {
      if ((payloadSize - cursor) < sizeof(DeltaRunHeader))
      {
        return false;
      }

      DeltaRunHeader run{};
      std::memcpy(&run, payload + cursor, sizeof(run));
      cursor += sizeof(run);

      if (run.length == 0)
      {
        return false;
      }
      if (static_cast<size_t>(run.offset) + static_cast<size_t>(run.length) > frameBufferSize)
      {
        return false;
      }
      if ((payloadSize - cursor) < run.length)
      {
        return false;
      }

      std::memcpy(frameBuffer + run.offset, payload + cursor, run.length);
      cursor += run.length;
    }
    return true;
  }

  default:
    return false;
  }
}

bool applyFramePayload(uint8_t encoding,
                       const uint8_t *payload,
                       size_t payloadSize,
                       uint8_t *frameBuffer,
                       size_t frameBufferSize)
{
  if (!isSupportedEncoding(encoding))
  {
    return false;
  }
  return applyFramePayload(static_cast<FrameEncoding>(encoding), payload, payloadSize, frameBuffer, frameBufferSize);
}

} // namespace video
