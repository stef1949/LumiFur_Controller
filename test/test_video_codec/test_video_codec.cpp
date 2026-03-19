#include <unity.h>

#include <cstring>

#include "core/video/MonoVideoCodec.h"

void setUp(void)
{
}

void tearDown(void)
{
}

void test_valid_header_is_accepted(void)
{
  video::MonoVideoHeader header{};
  std::memcpy(header.magic, video::kMonoVideoMagic, sizeof(video::kMonoVideoMagic));
  header.width = 128;
  header.height = 32;
  header.bytesPerFrame = static_cast<uint16_t>(video::bytesPerFrameForDimensions(header.width, header.height));
  header.frameCount = 100;
  header.frameIntervalMicros = 66667;

  TEST_ASSERT_TRUE(video::isValidHeader(header, 128, 32));
}

void test_invalid_header_rejects_bad_dimensions(void)
{
  video::MonoVideoHeader header{};
  std::memcpy(header.magic, video::kMonoVideoMagic, sizeof(video::kMonoVideoMagic));
  header.width = 127;
  header.height = 32;
  header.bytesPerFrame = 508;
  header.frameCount = 1;
  header.frameIntervalMicros = 33333;

  TEST_ASSERT_FALSE(video::isValidHeader(header));
}

void test_raw_frame_payload_replaces_frame_buffer(void)
{
  uint8_t frameBuffer[4] = {0x00, 0x00, 0x00, 0x00};
  const uint8_t payload[4] = {0xFF, 0xAA, 0x55, 0x11};

  TEST_ASSERT_TRUE(video::applyFramePayload(video::FrameEncoding::KeyframeRaw,
                                            payload,
                                            sizeof(payload),
                                            frameBuffer,
                                            sizeof(frameBuffer)));
  TEST_ASSERT_EQUAL_UINT8_ARRAY(payload, frameBuffer, sizeof(payload));
}

void test_delta_frame_payload_applies_multiple_runs(void)
{
  uint8_t frameBuffer[8] = {0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17};
  uint8_t payload[16] = {};

  video::DeltaRunHeader runA{1, 2};
  video::DeltaRunHeader runB{5, 3};
  std::memcpy(payload, &runA, sizeof(runA));
  payload[sizeof(runA) + 0] = 0xAA;
  payload[sizeof(runA) + 1] = 0xBB;
  std::memcpy(payload + sizeof(runA) + 2, &runB, sizeof(runB));
  payload[sizeof(runA) + 2 + sizeof(runB) + 0] = 0x01;
  payload[sizeof(runA) + 2 + sizeof(runB) + 1] = 0x02;
  payload[sizeof(runA) + 2 + sizeof(runB) + 2] = 0x03;

  const size_t payloadSize = sizeof(runA) + 2 + sizeof(runB) + 3;
  TEST_ASSERT_TRUE(video::applyFramePayload(video::FrameEncoding::DeltaRuns,
                                            payload,
                                            payloadSize,
                                            frameBuffer,
                                            sizeof(frameBuffer)));

  const uint8_t expected[8] = {0x10, 0xAA, 0xBB, 0x13, 0x14, 0x01, 0x02, 0x03};
  TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, frameBuffer, sizeof(expected));
}

void test_delta_frame_payload_rejects_out_of_bounds_runs(void)
{
  uint8_t frameBuffer[4] = {0x00, 0x00, 0x00, 0x00};
  uint8_t payload[8] = {};

  video::DeltaRunHeader run{3, 2};
  std::memcpy(payload, &run, sizeof(run));
  payload[sizeof(run) + 0] = 0xAA;
  payload[sizeof(run) + 1] = 0xBB;

  TEST_ASSERT_FALSE(video::applyFramePayload(video::FrameEncoding::DeltaRuns,
                                             payload,
                                             sizeof(run) + 2,
                                             frameBuffer,
                                             sizeof(frameBuffer)));
}

void test_delta_frame_payload_allows_empty_update(void)
{
  uint8_t frameBuffer[4] = {0x10, 0x20, 0x30, 0x40};
  const uint8_t expected[4] = {0x10, 0x20, 0x30, 0x40};
  const uint8_t dummyPayload = 0;

  TEST_ASSERT_TRUE(video::applyFramePayload(video::FrameEncoding::DeltaRuns,
                                            &dummyPayload,
                                            0,
                                            frameBuffer,
                                            sizeof(frameBuffer)));
  TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, frameBuffer, sizeof(expected));
}

void setup()
{
  UNITY_BEGIN();

  RUN_TEST(test_valid_header_is_accepted);
  RUN_TEST(test_invalid_header_rejects_bad_dimensions);
  RUN_TEST(test_raw_frame_payload_replaces_frame_buffer);
  RUN_TEST(test_delta_frame_payload_applies_multiple_runs);
  RUN_TEST(test_delta_frame_payload_rejects_out_of_bounds_runs);
  RUN_TEST(test_delta_frame_payload_allows_empty_update);

  UNITY_END();
}

int main()
{
  setup();
  return 0;
}

void loop()
{
}
