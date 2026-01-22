#include <unity.h>
#include "core/ColorParser.h"

void setUp(void)
{
}

void tearDown(void)
{
}

void test_trim_copy(void)
{
  TEST_ASSERT_EQUAL_STRING("abc", trimCopy("  abc\n").c_str());
  TEST_ASSERT_EQUAL_STRING("", trimCopy("   \t").c_str());
}

void test_trim_copy_no_change(void)
{
  TEST_ASSERT_EQUAL_STRING("abc", trimCopy("abc").c_str());
}

void test_parse_hex_digits(void)
{
  RgbColor color{};
  TEST_ASSERT_TRUE(parseHexColorDigits("FFA07C", color));
  TEST_ASSERT_EQUAL_UINT8(0xFF, color.r);
  TEST_ASSERT_EQUAL_UINT8(0xA0, color.g);
  TEST_ASSERT_EQUAL_UINT8(0x7C, color.b);
}

void test_parse_hex_digits_lowercase(void)
{
  RgbColor color{};
  TEST_ASSERT_TRUE(parseHexColorDigits("aa11bb", color));
  TEST_ASSERT_EQUAL_UINT8(0xAA, color.r);
  TEST_ASSERT_EQUAL_UINT8(0x11, color.g);
  TEST_ASSERT_EQUAL_UINT8(0xBB, color.b);
}

void test_parse_hex_digits_short(void)
{
  RgbColor color{};
  TEST_ASSERT_FALSE(parseHexColorDigits("ABC", color));
}

void test_parse_hex_digits_invalid(void)
{
  RgbColor color{};
  TEST_ASSERT_FALSE(parseHexColorDigits("GG0000", color));
}

void test_parse_decimal_components_with_clamping(void)
{
  RgbColor color{};
  TEST_ASSERT_TRUE(parseDecimalColorComponents("300,-10,128", color));
  TEST_ASSERT_EQUAL_UINT8(255, color.r);
  TEST_ASSERT_EQUAL_UINT8(0, color.g);
  TEST_ASSERT_EQUAL_UINT8(128, color.b);
}

void test_parse_decimal_components_with_separators(void)
{
  RgbColor color{};
  TEST_ASSERT_TRUE(parseDecimalColorComponents(" 1; 2 , 3 ", color));
  TEST_ASSERT_EQUAL_UINT8(1, color.r);
  TEST_ASSERT_EQUAL_UINT8(2, color.g);
  TEST_ASSERT_EQUAL_UINT8(3, color.b);
}

void test_parse_decimal_components_invalid_count(void)
{
  RgbColor color{};
  TEST_ASSERT_FALSE(parseDecimalColorComponents("1,2", color));
  TEST_ASSERT_FALSE(parseDecimalColorComponents("1,2,3,4", color));
}

void test_parse_decimal_components_invalid_chars(void)
{
  RgbColor color{};
  TEST_ASSERT_FALSE(parseDecimalColorComponents("x,2,3", color));
}

void test_parse_ascii_hex(void)
{
  RgbColor color{};
  TEST_ASSERT_TRUE(parseColorFromAscii("#12ab34", color));
  TEST_ASSERT_EQUAL_UINT8(0x12, color.r);
  TEST_ASSERT_EQUAL_UINT8(0xAB, color.g);
  TEST_ASSERT_EQUAL_UINT8(0x34, color.b);
}

void test_parse_ascii_hex_with_noise(void)
{
  RgbColor color{};
  TEST_ASSERT_TRUE(parseColorFromAscii("xx#12ab34zz", color));
  TEST_ASSERT_EQUAL_UINT8(0x12, color.r);
  TEST_ASSERT_EQUAL_UINT8(0xAB, color.g);
  TEST_ASSERT_EQUAL_UINT8(0x34, color.b);
}

void test_parse_ascii_decimal(void)
{
  RgbColor color{};
  TEST_ASSERT_TRUE(parseColorFromAscii("10, 20, 30", color));
  TEST_ASSERT_EQUAL_UINT8(10, color.r);
  TEST_ASSERT_EQUAL_UINT8(20, color.g);
  TEST_ASSERT_EQUAL_UINT8(30, color.b);
}

void test_parse_ascii_empty(void)
{
  RgbColor color{};
  TEST_ASSERT_FALSE(parseColorFromAscii("   \t", color));
}

void test_parse_ascii_invalid_decimal(void)
{
  RgbColor color{};
  TEST_ASSERT_FALSE(parseColorFromAscii("abc", color));
}

void test_color_to_hex_string(void)
{
  RgbColor color{0x01, 0xAF, 0x0B};
  TEST_ASSERT_EQUAL_STRING("01AF0B", colorToHexString(color).c_str());
}

void test_parse_raw_payload(void)
{
  uint8_t payload[3] = {1, 2, 3};
  RgbColor color{};
  std::string hex;
  TEST_ASSERT_TRUE(parseColorPayload(payload, sizeof(payload), color, hex));
  TEST_ASSERT_EQUAL_STRING("010203", hex.c_str());
  TEST_ASSERT_EQUAL_UINT8(1, color.r);
  TEST_ASSERT_EQUAL_UINT8(2, color.g);
  TEST_ASSERT_EQUAL_UINT8(3, color.b);
}

void test_parse_payload_ascii_hex(void)
{
  const char payload[] = "#0f10ff";
  RgbColor color{};
  std::string hex;
  TEST_ASSERT_TRUE(parseColorPayload(reinterpret_cast<const uint8_t *>(payload), sizeof(payload) - 1, color, hex));
  TEST_ASSERT_EQUAL_UINT8(0x0F, color.r);
  TEST_ASSERT_EQUAL_UINT8(0x10, color.g);
  TEST_ASSERT_EQUAL_UINT8(0xFF, color.b);
  TEST_ASSERT_EQUAL_STRING("0F10FF", hex.c_str());
}

void test_rejects_invalid_inputs(void)
{
  RgbColor color{};
  std::string hex;
  TEST_ASSERT_FALSE(parseColorPayload(nullptr, 0, color, hex));
  const char payload[] = "bad!";
  TEST_ASSERT_FALSE(parseColorPayload(reinterpret_cast<const uint8_t *>(payload), sizeof(payload) - 1, color, hex));
}

void setup()
{
  UNITY_BEGIN();
  RUN_TEST(test_trim_copy);
  RUN_TEST(test_trim_copy_no_change);
  RUN_TEST(test_parse_hex_digits);
  RUN_TEST(test_parse_hex_digits_lowercase);
  RUN_TEST(test_parse_hex_digits_short);
  RUN_TEST(test_parse_hex_digits_invalid);
  RUN_TEST(test_parse_decimal_components_with_clamping);
  RUN_TEST(test_parse_decimal_components_with_separators);
  RUN_TEST(test_parse_decimal_components_invalid_count);
  RUN_TEST(test_parse_decimal_components_invalid_chars);
  RUN_TEST(test_parse_ascii_hex);
  RUN_TEST(test_parse_ascii_hex_with_noise);
  RUN_TEST(test_parse_ascii_decimal);
  RUN_TEST(test_parse_ascii_empty);
  RUN_TEST(test_parse_ascii_invalid_decimal);
  RUN_TEST(test_color_to_hex_string);
  RUN_TEST(test_parse_raw_payload);
  RUN_TEST(test_parse_payload_ascii_hex);
  RUN_TEST(test_rejects_invalid_inputs);
  UNITY_END();
}

int main()
{
  setup();
  return 0;
}

void loop()
{
  // Unity tests run once
}
