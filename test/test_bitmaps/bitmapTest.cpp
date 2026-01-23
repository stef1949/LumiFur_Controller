#include <unity.h>
#include "bitmaps.h"

// Compute expected byte-size for a width x height monochrome XBM:
// each row is padded to full bytes: ((w + 7) / 8) bytes per row.
static constexpr size_t expectedBytes(int w, int h)
{
  return static_cast<size_t>(((w + 7) / 8) * h);
}

void setUp(void)
{
}

void tearDown(void)
{
}

void test_bluetoothBackground_size()
{
  // 7x11 -> (7+7)/8 = 1 byte/row, 11 rows = 11 bytes.
  TEST_ASSERT_EQUAL_UINT(expectedBytes(7, 11), sizeof(bluetoothBackground));
}

void test_bluetoothRune_size()
{
  // 5x7 -> (5+7)/8 = 1 byte/row, 7 rows = 7 bytes.
  TEST_ASSERT_EQUAL_UINT(expectedBytes(5, 7), sizeof(bluetoothRune));
}

void test_appleLogo_size()
{
  // 18x21 -> (18+7)/8 = 3 bytes/row, 21 rows = 63 bytes.
  TEST_ASSERT_EQUAL_UINT(expectedBytes(18, 21), sizeof(appleLogoApple_logo_black));
}

void test_dino_size()
{
  // 15x16 -> (15+7)/8 = 2 bytes/row, 16 rows = 32 bytes.
  TEST_ASSERT_EQUAL_UINT(expectedBytes(15, 16), sizeof(dino));
}

void test_DvDLogo_size()
{
  // 23x10 -> (23+7)/8 = 3 bytes/row, 10 rows = 30 bytes.
  TEST_ASSERT_EQUAL_UINT(expectedBytes(23, 10), sizeof(DvDLogo));
}

void setup()
{
  UNITY_BEGIN();
  RUN_TEST(test_bluetoothBackground_size);
  RUN_TEST(test_bluetoothRune_size);
  RUN_TEST(test_appleLogo_size);
  RUN_TEST(test_dino_size);
  RUN_TEST(test_DvDLogo_size);
  UNITY_END();
}

int main()
{
  setup();
  return 0;
}

void loop()
{
  // empty
}
