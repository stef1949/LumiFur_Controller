#include <unity.h>
#include "bitmaps.h"

// Compute expected byte‐size for a width×height monochrome XBM:
// each row is padded to full bytes: ((w+7)/8) bytes per row
static constexpr size_t expectedBytes(int w, int h) {
  return (size_t)(((w + 7) / 8) * h);
}

void test_bluetoothBackground_size() {
  // 10×12 → (10+7)/8=2 bytes/row ×12 rows = 24 bytes
  TEST_ASSERT_EQUAL_UINT(expectedBytes(10,12),
                        sizeof(bluetoothbluetoothBackground));
}

void test_bluetoothRune_size() {
  TEST_ASSERT_EQUAL_UINT(expectedBytes(10,12),
                        sizeof(bluetoothbluetoothRune));
}

void test_appleLogo_size() {
  // 18×21 → (18+7)/8=3 bytes/row ×21 rows = 63 bytes
  TEST_ASSERT_EQUAL_UINT(expectedBytes(18,21),
                        sizeof(appleLogoApple_logo_black));
}

void test_dino_size() {
  // 15×16 → (15+7)/8=2 bytes/row ×16 rows = 32 bytes
  TEST_ASSERT_EQUAL_UINT(expectedBytes(15,16),
                        sizeof(dino));
}

void test_DvDLogo_size() {
  // 23×10 → (23+7)/8=4 bytes/row ×10 rows = 40 bytes
  TEST_ASSERT_EQUAL_UINT(expectedBytes(23,10),
                        sizeof(DvDLogo));
}

void setup() {
  UNITY_BEGIN();
  RUN_TEST(test_bluetoothBackground_size);
  RUN_TEST(test_bluetoothRune_size);
  RUN_TEST(test_appleLogo_size);
  RUN_TEST(test_dino_size);
  RUN_TEST(test_DvDLogo_size);
  UNITY_END();
}

void loop() {
  // empty
}