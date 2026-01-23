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

void test_trim_copy_preserves_inner_whitespace(void)
{
  TEST_ASSERT_EQUAL_STRING("a b", trimCopy("  a b  ").c_str());
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

void test_parse_hex_digits_extra_chars(void)
{
  RgbColor color{};
  TEST_ASSERT_TRUE(parseHexColorDigits("ABCDEF12", color));
  TEST_ASSERT_EQUAL_UINT8(0xAB, color.r);
  TEST_ASSERT_EQUAL_UINT8(0xCD, color.g);
  TEST_ASSERT_EQUAL_UINT8(0xEF, color.b);
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

void test_parse_decimal_components_with_spaces(void)
{
  RgbColor color{};
  TEST_ASSERT_TRUE(parseDecimalColorComponents("5 6 7", color));
  TEST_ASSERT_EQUAL_UINT8(5, color.r);
  TEST_ASSERT_EQUAL_UINT8(6, color.g);
  TEST_ASSERT_EQUAL_UINT8(7, color.b);
}

void test_parse_decimal_components_with_plus_signs(void)
{
  RgbColor color{};
  TEST_ASSERT_TRUE(parseDecimalColorComponents("+1, +2, +3", color));
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

void test_parse_decimal_components_trailing_garbage(void)
{
  RgbColor color{};
  TEST_ASSERT_FALSE(parseDecimalColorComponents("1,2,3x", color));
}

void test_parse_decimal_components_trailing_separator(void)
{
  RgbColor color{};
  TEST_ASSERT_TRUE(parseDecimalColorComponents("1,2,3,", color));
  TEST_ASSERT_EQUAL_UINT8(1, color.r);
  TEST_ASSERT_EQUAL_UINT8(2, color.g);
  TEST_ASSERT_EQUAL_UINT8(3, color.b);
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

void test_parse_ascii_hex_with_commas(void)
{
  RgbColor color{};
  TEST_ASSERT_TRUE(parseColorFromAscii("#12,34,56", color));
  TEST_ASSERT_EQUAL_UINT8(0x12, color.r);
  TEST_ASSERT_EQUAL_UINT8(0x34, color.g);
  TEST_ASSERT_EQUAL_UINT8(0x56, color.b);
}

void test_parse_ascii_hex_with_separators_letters(void)
{
  RgbColor color{};
  TEST_ASSERT_TRUE(parseColorFromAscii("aa,bb,cc", color));
  TEST_ASSERT_EQUAL_UINT8(0xAA, color.r);
  TEST_ASSERT_EQUAL_UINT8(0xBB, color.g);
  TEST_ASSERT_EQUAL_UINT8(0xCC, color.b);
}

void test_parse_ascii_hex_plain(void)
{
  RgbColor color{};
  TEST_ASSERT_TRUE(parseColorFromAscii("FF00aa", color));
  TEST_ASSERT_EQUAL_UINT8(0xFF, color.r);
  TEST_ASSERT_EQUAL_UINT8(0x00, color.g);
  TEST_ASSERT_EQUAL_UINT8(0xAA, color.b);
}

void test_parse_ascii_hex_0x_prefix(void)
{
  RgbColor color{};
  TEST_ASSERT_TRUE(parseColorFromAscii("0x10AF20", color));
  TEST_ASSERT_EQUAL_UINT8(0x10, color.r);
  TEST_ASSERT_EQUAL_UINT8(0xAF, color.g);
  TEST_ASSERT_EQUAL_UINT8(0x20, color.b);
}

void test_parse_ascii_hex_0x_prefix_with_spaces(void)
{
  RgbColor color{};
  TEST_ASSERT_TRUE(parseColorFromAscii("0x12 34 56", color));
  TEST_ASSERT_EQUAL_UINT8(0x12, color.r);
  TEST_ASSERT_EQUAL_UINT8(0x34, color.g);
  TEST_ASSERT_EQUAL_UINT8(0x56, color.b);
}

void test_parse_ascii_hex_prefix_with_spaces(void)
{
  RgbColor color{};
  TEST_ASSERT_TRUE(parseColorFromAscii("#12 34 56", color));
  TEST_ASSERT_EQUAL_UINT8(0x12, color.r);
  TEST_ASSERT_EQUAL_UINT8(0x34, color.g);
  TEST_ASSERT_EQUAL_UINT8(0x56, color.b);
}

void test_parse_ascii_hex_prefix_too_short(void)
{
  RgbColor color{};
  TEST_ASSERT_FALSE(parseColorFromAscii("0x1", color));
}

void test_parse_ascii_hex_prefix_invalid(void)
{
  RgbColor color{};
  TEST_ASSERT_FALSE(parseColorFromAscii("#ZZ1122", color));
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

void test_parse_ascii_decimal_spaces_only(void)
{
  RgbColor color{};
  TEST_ASSERT_TRUE(parseColorFromAscii("12 34 56", color));
  TEST_ASSERT_EQUAL_UINT8(12, color.r);
  TEST_ASSERT_EQUAL_UINT8(34, color.g);
  TEST_ASSERT_EQUAL_UINT8(56, color.b);
}

void test_parse_ascii_digits_no_separators_hex(void)
{
  RgbColor color{};
  TEST_ASSERT_TRUE(parseColorFromAscii("010203", color));
  TEST_ASSERT_EQUAL_UINT8(0x01, color.r);
  TEST_ASSERT_EQUAL_UINT8(0x02, color.g);
  TEST_ASSERT_EQUAL_UINT8(0x03, color.b);
}

void test_parse_ascii_digits_short_rejected(void)
{
  RgbColor color{};
  TEST_ASSERT_FALSE(parseColorFromAscii("123", color));
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

void test_parse_raw_payload_full_range(void)
{
  uint8_t payload[3] = {0xAB, 0xCD, 0xEF};
  RgbColor color{};
  std::string hex;
  TEST_ASSERT_TRUE(parseColorPayload(payload, sizeof(payload), color, hex));
  TEST_ASSERT_EQUAL_STRING("ABCDEF", hex.c_str());
  TEST_ASSERT_EQUAL_UINT8(0xAB, color.r);
  TEST_ASSERT_EQUAL_UINT8(0xCD, color.g);
  TEST_ASSERT_EQUAL_UINT8(0xEF, color.b);
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

void test_parse_payload_ascii_decimal(void)
{
  const char payload[] = "255,0,128";
  RgbColor color{};
  std::string hex;
  TEST_ASSERT_TRUE(parseColorPayload(reinterpret_cast<const uint8_t *>(payload), sizeof(payload) - 1, color, hex));
  TEST_ASSERT_EQUAL_STRING("FF0080", hex.c_str());
  TEST_ASSERT_EQUAL_UINT8(255, color.r);
  TEST_ASSERT_EQUAL_UINT8(0, color.g);
  TEST_ASSERT_EQUAL_UINT8(128, color.b);
}

void test_parse_payload_ascii_invalid_length(void)
{
  const char payload[] = "GG";
  RgbColor color{};
  std::string hex;
  TEST_ASSERT_FALSE(parseColorPayload(reinterpret_cast<const uint8_t *>(payload), sizeof(payload) - 1, color, hex));
}

void test_parse_payload_binary_invalid_length(void)
{
  uint8_t payload[4] = {1, 2, 3, 4};
  RgbColor color{};
  std::string hex;
  TEST_ASSERT_FALSE(parseColorPayload(payload, sizeof(payload), color, hex));
}

void test_parse_payload_ascii_hex_prefix(void)
{
  const char payload[] = "0x0a0b0c";
  RgbColor color{};
  std::string hex;
  TEST_ASSERT_TRUE(parseColorPayload(reinterpret_cast<const uint8_t *>(payload), sizeof(payload) - 1, color, hex));
  TEST_ASSERT_EQUAL_STRING("0A0B0C", hex.c_str());
  TEST_ASSERT_EQUAL_UINT8(0x0A, color.r);
  TEST_ASSERT_EQUAL_UINT8(0x0B, color.g);
  TEST_ASSERT_EQUAL_UINT8(0x0C, color.b);
}

void test_parse_payload_invalid_length(void)
{
  uint8_t payload[2] = {1, 2};
  RgbColor color{};
  std::string hex;
  TEST_ASSERT_FALSE(parseColorPayload(payload, sizeof(payload), color, hex));
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
  RUN_TEST(test_trim_copy_preserves_inner_whitespace);
  RUN_TEST(test_parse_hex_digits);
  RUN_TEST(test_parse_hex_digits_lowercase);
  RUN_TEST(test_parse_hex_digits_short);
  RUN_TEST(test_parse_hex_digits_invalid);
  RUN_TEST(test_parse_hex_digits_extra_chars);
  RUN_TEST(test_parse_decimal_components_with_clamping);
  RUN_TEST(test_parse_decimal_components_with_separators);
  RUN_TEST(test_parse_decimal_components_with_spaces);
  RUN_TEST(test_parse_decimal_components_with_plus_signs);
  RUN_TEST(test_parse_decimal_components_invalid_count);
  RUN_TEST(test_parse_decimal_components_invalid_chars);
  RUN_TEST(test_parse_decimal_components_trailing_garbage);
  RUN_TEST(test_parse_decimal_components_trailing_separator);
  RUN_TEST(test_parse_ascii_hex);
  RUN_TEST(test_parse_ascii_hex_with_noise);
  RUN_TEST(test_parse_ascii_hex_with_commas);
  RUN_TEST(test_parse_ascii_hex_with_separators_letters);
  RUN_TEST(test_parse_ascii_hex_plain);
  RUN_TEST(test_parse_ascii_hex_0x_prefix);
  RUN_TEST(test_parse_ascii_hex_0x_prefix_with_spaces);
  RUN_TEST(test_parse_ascii_hex_prefix_with_spaces);
  RUN_TEST(test_parse_ascii_hex_prefix_too_short);
  RUN_TEST(test_parse_ascii_hex_prefix_invalid);
  RUN_TEST(test_parse_ascii_decimal);
  RUN_TEST(test_parse_ascii_empty);
  RUN_TEST(test_parse_ascii_decimal_spaces_only);
  RUN_TEST(test_parse_ascii_digits_no_separators_hex);
  RUN_TEST(test_parse_ascii_digits_short_rejected);
  RUN_TEST(test_parse_ascii_invalid_decimal);
  RUN_TEST(test_color_to_hex_string);
  RUN_TEST(test_parse_raw_payload);
  RUN_TEST(test_parse_raw_payload_full_range);
  RUN_TEST(test_parse_payload_ascii_hex);
  RUN_TEST(test_parse_payload_ascii_decimal);
  RUN_TEST(test_parse_payload_ascii_invalid_length);
  RUN_TEST(test_parse_payload_binary_invalid_length);
  RUN_TEST(test_parse_payload_ascii_hex_prefix);
  RUN_TEST(test_parse_payload_invalid_length);
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
