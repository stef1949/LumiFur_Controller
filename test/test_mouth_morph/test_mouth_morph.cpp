#include <unity.h>

#include <cstdint>
#include <cstring>

#include "core/mouth/MouthMorph.h"

void setUp(void)
{
}

void tearDown(void)
{
}

void test_openness_quantization_preserves_endpoints(void)
{
  TEST_ASSERT_EQUAL_UINT8(0, mouth::quantizeOpenness(0));
  TEST_ASSERT_EQUAL_UINT8(mouth::kFrameCount - 1, mouth::quantizeOpenness(255));
  TEST_ASSERT_EQUAL_UINT8(0, mouth::opennessForFrame(0));
  TEST_ASSERT_EQUAL_UINT8(255, mouth::opennessForFrame(mouth::kFrameCount - 1));
}

void test_vertical_warp_keeps_lower_jaw_anchored(void)
{
  TEST_ASSERT_EQUAL_INT(-1, mouth::warpedSourceRow(8, 22, 13, 0));
  TEST_ASSERT_EQUAL_INT(0, mouth::warpedSourceRow(9, 22, 13, 0));
  TEST_ASSERT_EQUAL_INT(21, mouth::warpedSourceRow(21, 22, 13, 0));
  TEST_ASSERT_EQUAL_INT(0, mouth::warpedSourceRow(0, 22, 13, 255));
  TEST_ASSERT_EQUAL_INT(21, mouth::warpedSourceRow(21, 22, 13, 255));
}

void test_interpolation_preserves_source_endpoints(void)
{
  constexpr std::uint8_t closed[] = {0x00, 0x00, 0x3c, 0x18};
  constexpr std::uint8_t open[] = {0x18, 0x3c, 0x7e, 0xff};
  std::uint8_t output[sizeof(closed)] = {};

  TEST_ASSERT_TRUE(mouth::buildInterpolatedXbm(
      closed, open, 8, 4, 2, 0, false, output, sizeof(output)));
  TEST_ASSERT_EQUAL_MEMORY(closed, output, sizeof(closed));

  TEST_ASSERT_TRUE(mouth::buildInterpolatedXbm(
      closed, open, 8, 4, 2, 255, false, output, sizeof(output)));
  TEST_ASSERT_EQUAL_MEMORY(open, output, sizeof(open));
}

void test_mirrored_halves_use_mirrored_dither(void)
{
  constexpr std::uint8_t closed[] = {0x00};
  constexpr std::uint8_t open[] = {0xff};
  std::uint8_t right[1] = {};
  std::uint8_t left[1] = {};

  TEST_ASSERT_TRUE(mouth::buildInterpolatedXbm(
      closed, open, 8, 1, 1, 128, false, right, sizeof(right)));
  TEST_ASSERT_TRUE(mouth::buildInterpolatedXbm(
      closed, open, 8, 1, 1, 128, true, left, sizeof(left)));
  TEST_ASSERT_EQUAL_HEX8(0xaa, right[0]);
  TEST_ASSERT_EQUAL_HEX8(0x55, left[0]);
}

void test_interpolation_rejects_undersized_output(void)
{
  constexpr std::uint8_t closed[] = {0x00, 0x00};
  constexpr std::uint8_t open[] = {0xff, 0xff};
  std::uint8_t output[1] = {};

  TEST_ASSERT_FALSE(mouth::buildInterpolatedXbm(
      closed, open, 8, 2, 1, 128, false, output, sizeof(output)));
}

void setup()
{
  UNITY_BEGIN();
  RUN_TEST(test_openness_quantization_preserves_endpoints);
  RUN_TEST(test_vertical_warp_keeps_lower_jaw_anchored);
  RUN_TEST(test_interpolation_preserves_source_endpoints);
  RUN_TEST(test_mirrored_halves_use_mirrored_dither);
  RUN_TEST(test_interpolation_rejects_undersized_output);
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
