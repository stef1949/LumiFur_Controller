#pragma once

#include <Arduino.h>
#include <FS.h>

#include "core/video/MonoVideoCodec.h"

class MatrixPanel_I2S_DMA;

struct MonoVideoDebugInfo
{
  bool fileOpen = false;
  bool frameLoaded = false;
  uint32_t currentFrameIndex = 0;
  uint32_t frameCount = 0;
  uint32_t frameIntervalMicros = 0;
  uint32_t rewindCount = 0;
  uint32_t decodeFailureCount = 0;
  uint32_t lastDecodeMicros = 0;
  uint32_t currentFileOffset = 0;
  uint8_t lastCatchupFrames = 0;
  uint8_t maxCatchupFrames = 0;
};

class MonoVideoPlayer
{
public:
  MonoVideoPlayer(fs::FS &filesystem, const char *path, uint16_t width, uint16_t height, uint16_t panelWidth = 0, bool debugEnabled = false);
  ~MonoVideoPlayer();

  void begin();
  void updateAndDraw(MatrixPanel_I2S_DMA *display, uint32_t nowMicros);

  bool ready() const;
  const char *error() const;
  MonoVideoDebugInfo debugInfo() const;

private:
  bool openVideo();
  bool tryOpenVideoPath(const char *candidatePath);
  bool rewindAndDecode(uint32_t nowMicros);
  bool decodeNextFrame();
  void closeVideo();
  void clearState();
  void resetDebugCounters();
  void setError(const char *message);
  void drawCurrentFrame(MatrixPanel_I2S_DMA *display) const;
  void drawError(MatrixPanel_I2S_DMA *display) const;
  void logDebug(const char *format, ...) const;

  fs::FS &filesystem_;
  const char *path_;
  const uint16_t expectedWidth_;
  const uint16_t expectedHeight_;
  const uint16_t panelWidth_;
  const size_t frameBufferSize_;
  const size_t payloadBufferSize_;
  const bool debugEnabled_;

  File file_;
  video::MonoVideoHeader header_{};
  uint8_t *frameBuffer_ = nullptr;
  uint8_t *payloadBuffer_ = nullptr;
  bool fileOpen_ = false;
  bool frameLoaded_ = false;
  uint32_t lastFrameMicros_ = 0;
  uint32_t currentFrameIndex_ = 0;
  uint32_t rewindCount_ = 0;
  uint32_t decodeFailureCount_ = 0;
  uint32_t lastDecodeMicros_ = 0;
  uint32_t currentFileOffset_ = 0;
  uint8_t lastCatchupFrames_ = 0;
  uint8_t maxCatchupFrames_ = 0;
  bool lastDecodeReachedEof_ = false;
  char activePath_[96] = "";
  char errorText_[24] = "Video unavailable";
};
