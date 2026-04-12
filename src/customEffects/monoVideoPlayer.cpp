#include "customEffects/monoVideoPlayer.h"

#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <Fonts/TomThumb.h>

#include <algorithm>
#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace
{

constexpr uint8_t kPayloadExpansionFactor = 5;
constexpr uint16_t kForegroundColor = 0xFFFF;
constexpr uint16_t kErrorColor = 0xF800;
constexpr uint8_t kMaxCatchupFrames = 4;
constexpr char kVideoDirectory[] = "/videos";

bool hasCaseInsensitiveSuffix(const char *text, const char *suffix)
{
  if (!text || !suffix)
  {
    return false;
  }

  const size_t textLength = std::strlen(text);
  const size_t suffixLength = std::strlen(suffix);
  if (suffixLength > textLength)
  {
    return false;
  }

  const char *textSuffix = text + (textLength - suffixLength);
  for (size_t index = 0; index < suffixLength; ++index)
  {
    const char textChar = static_cast<char>(std::tolower(static_cast<unsigned char>(textSuffix[index])));
    const char suffixChar = static_cast<char>(std::tolower(static_cast<unsigned char>(suffix[index])));
    if (textChar != suffixChar)
    {
      return false;
    }
  }

  return true;
}

void copyString(char *destination, size_t destinationSize, const char *source)
{
  if (!destination || destinationSize == 0)
  {
    return;
  }

  if (!source)
  {
    destination[0] = '\0';
    return;
  }

  std::strncpy(destination, source, destinationSize - 1);
  destination[destinationSize - 1] = '\0';
}

void buildVideoPath(const char *entryName, char *destination, size_t destinationSize)
{
  if (!destination || destinationSize == 0)
  {
    return;
  }

  if (!entryName || entryName[0] == '\0')
  {
    destination[0] = '\0';
    return;
  }

  if (entryName[0] == '/')
  {
    copyString(destination, destinationSize, entryName);
    return;
  }

  snprintf(destination, destinationSize, "%s/%s", kVideoDirectory, entryName);
}

} // namespace

MonoVideoPlayer::MonoVideoPlayer(fs::FS &filesystem, const char *path, uint16_t width, uint16_t height, uint16_t panelWidth, bool debugEnabled)
    : filesystem_(filesystem),
      path_(path),
      expectedWidth_(width),
      expectedHeight_(height),
      panelWidth_(panelWidth),
      frameBufferSize_(video::bytesPerFrameForDimensions(width, height)),
      payloadBufferSize_(frameBufferSize_ * kPayloadExpansionFactor),
      debugEnabled_(debugEnabled)
{
  frameBuffer_ = new uint8_t[frameBufferSize_];
  payloadBuffer_ = new uint8_t[payloadBufferSize_];
  clearState();
  resetDebugCounters();
}

MonoVideoPlayer::~MonoVideoPlayer()
{
  closeVideo();
  delete[] frameBuffer_;
  delete[] payloadBuffer_;
}

void MonoVideoPlayer::clearState()
{
  if (frameBuffer_)
  {
    std::memset(frameBuffer_, 0, frameBufferSize_);
  }
  std::memset(&header_, 0, sizeof(header_));
  frameLoaded_ = false;
  lastFrameMicros_ = 0;
  currentFileOffset_ = 0;
  lastCatchupFrames_ = 0;
  activePath_[0] = '\0';
}

void MonoVideoPlayer::resetDebugCounters()
{
  currentFrameIndex_ = 0;
  rewindCount_ = 0;
  decodeFailureCount_ = 0;
  lastDecodeMicros_ = 0;
  currentFileOffset_ = 0;
  lastCatchupFrames_ = 0;
  maxCatchupFrames_ = 0;
  lastDecodeReachedEof_ = false;
}

void MonoVideoPlayer::setError(const char *message)
{
  if (!message)
  {
    message = "Video error";
  }
  std::strncpy(errorText_, message, sizeof(errorText_) - 1);
  errorText_[sizeof(errorText_) - 1] = '\0';
  Serial.printf("Video player: %s (%s)\n", errorText_, path_);
}

void MonoVideoPlayer::logDebug(const char *format, ...) const
{
  if (!debugEnabled_ || !format)
  {
    return;
  }

  char buffer[192];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);

  Serial.print("[video] ");
  Serial.println(buffer);
}

bool MonoVideoPlayer::tryOpenVideoPath(const char *candidatePath)
{
  File candidate = filesystem_.open(candidatePath, FILE_READ);
  if (!candidate || candidate.isDirectory())
  {
    return false;
  }

  video::MonoVideoHeader candidateHeader{};
  const size_t headerBytes = candidate.read(reinterpret_cast<uint8_t *>(&candidateHeader), sizeof(candidateHeader));
  if (headerBytes != sizeof(candidateHeader))
  {
    candidate.close();
    setError("Short video header");
    return false;
  }
  if (!video::isValidHeader(candidateHeader))
  {
    candidate.close();
    setError("Bad video format");
    return false;
  }
  const bool widthMatchesDisplay = candidateHeader.width == expectedWidth_;
  const bool widthFitsPanel = panelWidth_ > 0 && candidateHeader.width <= panelWidth_;
  if (!widthMatchesDisplay && !widthFitsPanel)
  {
    candidate.close();
    setError("Video width mismatch");
    return false;
  }
  if (candidateHeader.height > expectedHeight_)
  {
    candidate.close();
    setError("Video height mismatch");
    return false;
  }
  if (candidateHeader.bytesPerFrame > frameBufferSize_)
  {
    candidate.close();
    setError("Video size mismatch");
    return false;
  }

  file_ = candidate;
  header_ = candidateHeader;
  fileOpen_ = true;
  currentFileOffset_ = sizeof(candidateHeader);
  copyString(activePath_, sizeof(activePath_), candidatePath);
  logDebug("opened %s: %ux%u frames=%lu interval=%luus bytes=%u",
           activePath_,
           static_cast<unsigned int>(header_.width),
           static_cast<unsigned int>(header_.height),
           static_cast<unsigned long>(header_.frameCount),
           static_cast<unsigned long>(header_.frameIntervalMicros),
           static_cast<unsigned int>(header_.bytesPerFrame));
  return true;
}

bool MonoVideoPlayer::openVideo()
{
  if (fileOpen_)
  {
    return true;
  }

  if (tryOpenVideoPath(path_))
  {
    return true;
  }

  char fallbackPath[sizeof(activePath_)] = "";
  File directory = filesystem_.open(kVideoDirectory);
  if (!directory || !directory.isDirectory())
  {
    setError("Missing /videos dir");
    logDebug("open failed: %s missing and %s unavailable", path_, kVideoDirectory);
    return false;
  }

  for (File entry = directory.openNextFile(); entry; entry = directory.openNextFile())
  {
    const char *entryName = entry.name();
    if (entry.isDirectory() || !hasCaseInsensitiveSuffix(entryName, ".lfv"))
    {
      entry.close();
      continue;
    }

    buildVideoPath(entryName, fallbackPath, sizeof(fallbackPath));
    entry.close();

    if (std::strcmp(fallbackPath, path_) == 0)
    {
      continue;
    }

    if (tryOpenVideoPath(fallbackPath))
    {
      logDebug("using fallback video %s because %s was unavailable", fallbackPath, path_);
      return true;
    }
  }

  setError("No playable .lfv");
  logDebug("open failed: no playable .lfv found in %s", kVideoDirectory);
  return false;
}

void MonoVideoPlayer::closeVideo()
{
  if (file_)
  {
    file_.close();
  }
  fileOpen_ = false;
}

bool MonoVideoPlayer::decodeNextFrame()
{
  if (!fileOpen_ || !payloadBuffer_)
  {
    logDebug("decode skipped: file not open or payload buffer missing");
    return false;
  }

  const uint32_t decodeStartMicros = micros();
  lastDecodeReachedEof_ = false;
  video::MonoVideoFrameHeader frameHeader{};
  const size_t frameHeaderBytes = file_.read(reinterpret_cast<uint8_t *>(&frameHeader), sizeof(frameHeader));
  if (frameHeaderBytes != sizeof(frameHeader))
  {
    lastDecodeReachedEof_ = (frameHeaderBytes == 0U) && !file_.available();
    if (!lastDecodeReachedEof_)
    {
      ++decodeFailureCount_;
      logDebug("frame header read failed at offset=%lu", static_cast<unsigned long>(currentFileOffset_));
    }
    return false;
  }
  currentFileOffset_ += sizeof(frameHeader);
  // A delta frame may legitimately contain no payload when it is identical to the previous frame.
  if (frameHeader.payloadSize > payloadBufferSize_)
  {
    ++decodeFailureCount_;
    logDebug("invalid payload size=%u max=%u frame=%lu",
             static_cast<unsigned int>(frameHeader.payloadSize),
             static_cast<unsigned int>(payloadBufferSize_),
             static_cast<unsigned long>(currentFrameIndex_));
    return false;
  }

  const size_t payloadBytes = file_.read(payloadBuffer_, frameHeader.payloadSize);
  if (payloadBytes != frameHeader.payloadSize)
  {
    ++decodeFailureCount_;
    logDebug("payload read failed: expected=%u got=%u frame=%lu",
             static_cast<unsigned int>(frameHeader.payloadSize),
             static_cast<unsigned int>(payloadBytes),
             static_cast<unsigned long>(currentFrameIndex_));
    return false;
  }
  currentFileOffset_ += frameHeader.payloadSize;

  if (!video::applyFramePayload(frameHeader.encoding, payloadBuffer_, frameHeader.payloadSize, frameBuffer_, header_.bytesPerFrame))
  {
    ++decodeFailureCount_;
    logDebug("payload decode failed: encoding=%u size=%u frame=%lu",
             static_cast<unsigned int>(frameHeader.encoding),
             static_cast<unsigned int>(frameHeader.payloadSize),
             static_cast<unsigned long>(currentFrameIndex_));
    return false;
  }

  lastDecodeMicros_ = micros() - decodeStartMicros;
  return true;
}

bool MonoVideoPlayer::rewindAndDecode(uint32_t nowMicros)
{
  closeVideo();
  clearState();
  ++rewindCount_;
  logDebug("rewind #%lu", static_cast<unsigned long>(rewindCount_));
  if (!openVideo())
  {
    return false;
  }
  if (!decodeNextFrame())
  {
    closeVideo();
    setError("Video decode failed");
    return false;
  }

  frameLoaded_ = true;
  lastFrameMicros_ = nowMicros;
  currentFrameIndex_ = 0;
  return true;
}

void MonoVideoPlayer::begin()
{
  resetDebugCounters();
  closeVideo();
  clearState();
  if (!openVideo())
  {
    return;
  }
  if (!decodeNextFrame())
  {
    closeVideo();
    setError("Video decode failed");
    return;
  }

  frameLoaded_ = true;
  lastFrameMicros_ = micros();
  currentFrameIndex_ = 0;
}

void MonoVideoPlayer::drawCurrentFrame(MatrixPanel_I2S_DMA *display) const
{
  if (!display || !frameBuffer_ || header_.width == 0 || header_.height == 0)
  {
    return;
  }

  const size_t bytesPerRow = static_cast<size_t>(header_.width) / 8U;
  const int16_t verticalOffset = static_cast<int16_t>((expectedHeight_ - header_.height) / 2U);
  if (panelWidth_ == 0)
  {
    const int16_t horizontalOffset = static_cast<int16_t>((expectedWidth_ - header_.width) / 2U);
    for (uint16_t y = 0; y < header_.height; ++y)
    {
      const uint8_t *row = frameBuffer_ + (static_cast<size_t>(y) * bytesPerRow);
      int16_t runStart = -1;

      for (uint16_t x = 0; x < header_.width; ++x)
      {
        const uint8_t packed = row[x >> 3];
        const bool lit = (packed & (0x80U >> (x & 7U))) != 0U;

        if (lit)
        {
          if (runStart < 0)
          {
            runStart = static_cast<int16_t>(x);
          }
        }
        else if (runStart >= 0)
        {
          display->drawFastHLine(horizontalOffset + runStart, verticalOffset + y, x - runStart, kForegroundColor);
          runStart = -1;
        }
      }

      if (runStart >= 0)
      {
        display->drawFastHLine(horizontalOffset + runStart, verticalOffset + y, header_.width - runStart, kForegroundColor);
      }
    }
    return;
  }

  if (header_.width <= panelWidth_)
  {
    for (uint16_t y = 0; y < header_.height; ++y)
    {
      const uint8_t *row = frameBuffer_ + (static_cast<size_t>(y) * bytesPerRow);
      int16_t runStart = -1;

      for (uint16_t x = 0; x < header_.width; ++x)
      {
        const uint8_t packed = row[x >> 3];
        const bool lit = (packed & (0x80U >> (x & 7U))) != 0U;

        if (lit)
        {
          if (runStart < 0)
          {
            runStart = static_cast<int16_t>(x);
          }
        }
        else if (runStart >= 0)
        {
          const uint16_t runLength = x - runStart;
          const uint16_t runEnd = x - 1U;
          const uint16_t mirroredRunStart = (header_.width - 1U) - runEnd;
          for (uint16_t panelStart = 0; panelStart < expectedWidth_; panelStart += panelWidth_)
          {
            const int16_t horizontalOffset = static_cast<int16_t>(panelStart + ((panelWidth_ - header_.width) / 2U));
            display->drawFastHLine(horizontalOffset + mirroredRunStart, verticalOffset + y, runLength, kForegroundColor);
          }
          runStart = -1;
        }
      }

      if (runStart >= 0)
      {
        const uint16_t runLength = header_.width - runStart;
        const uint16_t runEnd = header_.width - 1U;
        const uint16_t mirroredRunStart = (header_.width - 1U) - runEnd;
        for (uint16_t panelStart = 0; panelStart < expectedWidth_; panelStart += panelWidth_)
        {
          const int16_t horizontalOffset = static_cast<int16_t>(panelStart + ((panelWidth_ - header_.width) / 2U));
          display->drawFastHLine(horizontalOffset + mirroredRunStart, verticalOffset + y, runLength, kForegroundColor);
        }
      }
    }
    return;
  }

  const uint16_t segmentWidth = std::min(panelWidth_, header_.width);
  for (uint16_t y = 0; y < header_.height; ++y)
  {
    const uint8_t *row = frameBuffer_ + (static_cast<size_t>(y) * bytesPerRow);

    for (uint16_t segmentStart = 0; segmentStart < header_.width; segmentStart += segmentWidth)
    {
      const uint16_t segmentEnd = std::min<uint16_t>(header_.width, segmentStart + segmentWidth);
      const uint16_t localSegmentWidth = segmentEnd - segmentStart;
      int16_t runStart = -1;

      for (uint16_t x = segmentStart; x < segmentEnd; ++x)
      {
        const uint8_t packed = row[x >> 3];
        const bool lit = (packed & (0x80U >> (x & 7U))) != 0U;

        if (lit)
        {
          if (runStart < 0)
          {
            runStart = static_cast<int16_t>(x);
          }
        }
        else if (runStart >= 0)
        {
          const uint16_t runEnd = x - 1U;
          const uint16_t mirroredRunStart = segmentStart + (localSegmentWidth - 1U) - (runEnd - segmentStart);
          display->drawFastHLine(mirroredRunStart, verticalOffset + y, x - runStart, kForegroundColor);
          runStart = -1;
        }
      }

      if (runStart >= 0)
      {
        const uint16_t runEnd = segmentEnd - 1U;
        const uint16_t mirroredRunStart = segmentStart + (localSegmentWidth - 1U) - (runEnd - segmentStart);
        display->drawFastHLine(mirroredRunStart, verticalOffset + y, segmentEnd - runStart, kForegroundColor);
      }
    }
  }
}

void MonoVideoPlayer::drawError(MatrixPanel_I2S_DMA *display) const
{
  if (!display)
  {
    return;
  }

  display->setFont(&TomThumb);
  display->setTextSize(1);
  display->setTextColor(kErrorColor);
  display->setCursor(3, 12);
  display->print("Video Err");
  display->setCursor(3, 22);
  display->print(errorText_);
}

void MonoVideoPlayer::updateAndDraw(MatrixPanel_I2S_DMA *display, uint32_t nowMicros)
{
  if (!frameLoaded_)
  {
    if (!rewindAndDecode(nowMicros))
    {
      drawError(display);
      return;
    }
  }

  uint8_t catchupFrames = 0;
  while (header_.frameIntervalMicros > 0 &&
         static_cast<uint32_t>(nowMicros - lastFrameMicros_) >= header_.frameIntervalMicros &&
         catchupFrames < kMaxCatchupFrames)
  {
    if (!decodeNextFrame())
    {
      if (lastDecodeReachedEof_)
      {
        logDebug("end of video reached at frame=%lu, rewinding",
                 static_cast<unsigned long>(currentFrameIndex_));
      }
      else
      {
        logDebug("decode failed during playback at frame=%lu offset=%lu, rewinding",
                 static_cast<unsigned long>(currentFrameIndex_),
                 static_cast<unsigned long>(currentFileOffset_));
      }
      if (!rewindAndDecode(nowMicros))
      {
        drawError(display);
        return;
      }
      break;
    }

    if ((currentFrameIndex_ + 1U) < header_.frameCount)
    {
      ++currentFrameIndex_;
    }
    lastFrameMicros_ += header_.frameIntervalMicros;
    ++catchupFrames;
  }

  lastCatchupFrames_ = catchupFrames;
  if (catchupFrames > maxCatchupFrames_)
  {
    maxCatchupFrames_ = catchupFrames;
  }
  if (catchupFrames == kMaxCatchupFrames &&
      header_.frameIntervalMicros > 0 &&
      static_cast<uint32_t>(nowMicros - lastFrameMicros_) >= header_.frameIntervalMicros)
  {
    logDebug("frame pacing fell behind by %luus; resyncing clock",
             static_cast<unsigned long>(nowMicros - lastFrameMicros_));
    lastFrameMicros_ = nowMicros;
  }

  drawCurrentFrame(display);
}

bool MonoVideoPlayer::ready() const
{
  return frameLoaded_;
}

const char *MonoVideoPlayer::error() const
{
  return errorText_;
}

MonoVideoDebugInfo MonoVideoPlayer::debugInfo() const
{
  MonoVideoDebugInfo info;
  info.fileOpen = fileOpen_;
  info.frameLoaded = frameLoaded_;
  info.currentFrameIndex = currentFrameIndex_;
  info.frameCount = header_.frameCount;
  info.frameIntervalMicros = header_.frameIntervalMicros;
  info.rewindCount = rewindCount_;
  info.decodeFailureCount = decodeFailureCount_;
  info.lastDecodeMicros = lastDecodeMicros_;
  info.currentFileOffset = currentFileOffset_;
  info.lastCatchupFrames = lastCatchupFrames_;
  info.maxCatchupFrames = maxCatchupFrames_;
  return info;
}
