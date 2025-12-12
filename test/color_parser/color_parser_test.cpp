#include <gtest/gtest.h>

#include "core/ColorParser.h"

TEST(ColorParser, TrimsWhitespace)
{
  EXPECT_EQ("abc", trimCopy("  abc\n"));
  EXPECT_EQ("", trimCopy("   \t"));
}

TEST(ColorParser, ParsesHexDigits)
{
  RgbColor color;
  EXPECT_TRUE(parseHexColorDigits("FFA07C", color));
  EXPECT_EQ(0xFF, color.r);
  EXPECT_EQ(0xA0, color.g);
  EXPECT_EQ(0x7C, color.b);
}

TEST(ColorParser, ParsesDecimalComponentsWithClamping)
{
  RgbColor color;
  EXPECT_TRUE(parseDecimalColorComponents("300,-10,128", color));
  EXPECT_EQ(255, color.r);
  EXPECT_EQ(0, color.g);
  EXPECT_EQ(128, color.b);
}

TEST(ColorParser, ParsesAsciiHex)
{
  RgbColor color;
  EXPECT_TRUE(parseColorFromAscii("#12ab34", color));
  EXPECT_EQ(0x12, color.r);
  EXPECT_EQ(0xAB, color.g);
  EXPECT_EQ(0x34, color.b);
}

TEST(ColorParser, ParsesAsciiDecimal)
{
  RgbColor color;
  EXPECT_TRUE(parseColorFromAscii("10, 20, 30", color));
  EXPECT_EQ(10, color.r);
  EXPECT_EQ(20, color.g);
  EXPECT_EQ(30, color.b);
}

TEST(ColorParser, ParsesRawPayload)
{
  uint8_t payload[3] = {1, 2, 3};
  RgbColor color;
  std::string hex;
  EXPECT_TRUE(parseColorPayload(payload, sizeof(payload), color, hex));
  EXPECT_EQ("010203", hex);
  EXPECT_EQ(1, color.r);
  EXPECT_EQ(2, color.g);
  EXPECT_EQ(3, color.b);
}

TEST(ColorParser, RejectsInvalidInputs)
{
  RgbColor color;
  std::string hex;
  EXPECT_FALSE(parseColorPayload(nullptr, 0, color, hex));
  EXPECT_FALSE(parseColorPayload(reinterpret_cast<const uint8_t *>("bad"), 3, color, hex));
}

