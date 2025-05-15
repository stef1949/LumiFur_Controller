#include <unity.h>
#include "main.h"

// Externs for testable globals and functions
extern bool useShakeSensitivity;
extern const float SLEEP_THRESHOLD;
extern const float SHAKE_THRESHOLD;
extern volatile unsigned long softMillis;
extern void onSoftMillisTimer(TimerHandle_t);

void test_getCurrentThreshold_shake(void) {
  useShakeSensitivity = true;
  float thr = getCurrentThreshold();
  TEST_ASSERT_FLOAT_WITHIN(0.0001, SHAKE_THRESHOLD, thr);
}

void test_getCurrentThreshold_sleep(void) {
  useShakeSensitivity = false;
  float thr = getCurrentThreshold();
  TEST_ASSERT_FLOAT_WITHIN(0.0001, SLEEP_THRESHOLD, thr);
}

void test_onSoftMillisTimer_increments(void) {
  unsigned long before = softMillis;
  onSoftMillisTimer(nullptr);
  TEST_ASSERT_EQUAL_UINT(before + 1, softMillis);
}

void setup() {
  UNITY_BEGIN();
  RUN_TEST(test_getCurrentThreshold_shake);
  RUN_TEST(test_getCurrentThreshold_sleep);
  RUN_TEST(test_onSoftMillisTimer_increments);
  UNITY_END();
}

void loop() {
  // no-op
}#include <unity.h>
#include "main.h"

// Externs
extern bool useShakeSensitivity;
extern float SLEEP_THRESHOLD;
extern float SHAKE_THRESHOLD;
extern volatile unsigned long softMillis;
extern void onSoftMillisTimer(TimerHandle_t);

// Test getCurrentThreshold() for shake sensitivity
void test_getCurrentThreshold_shake(void) {
  useShakeSensitivity = true;
  float thr = getCurrentThreshold();
  TEST_ASSERT_FLOAT_WITHIN(0.0001, SHAKE_THRESHOLD, thr);
}

// Test getCurrentThreshold() for sleep sensitivity
void test_getCurrentThreshold_sleep(void) {
  useShakeSensitivity = false;
  float thr = getCurrentThreshold();
  TEST_ASSERT_FLOAT_WITHIN(0.0001, SLEEP_THRESHOLD, thr);
}

// Test that onSoftMillisTimer increments softMillis
void test_onSoftMillisTimer_increments(void) {
  unsigned long before = softMillis;
  onSoftMillisTimer(nullptr);
  TEST_ASSERT_EQUAL_UINT(before + 1, softMillis);
}

void setup() {
  UNITY_BEGIN();
  RUN_TEST(test_getCurrentThreshold_shake);
  RUN_TEST(test_getCurrentThreshold_sleep);
  RUN_TEST(test_onSoftMillisTimer_increments);
  UNITY_END();
}

void loop() {
  // no-op
}