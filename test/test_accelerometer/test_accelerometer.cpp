#include <unity.h>
#include <cmath>
#include <cstdint>

// Constants from main.cpp
const float SLEEP_THRESHOLD = 1.0f;
const float SHAKE_THRESHOLD = 1.0f;

// Test globals
bool useShakeSensitivity = false;
float lastAccelDeltaX = 0.0f;
float lastAccelDeltaY = 0.0f;
float lastAccelDeltaZ = 0.0f;
bool lastMotionAboveShake = false;
bool lastMotionAboveSleep = false;

// Functions under test
float getCurrentThreshold()
{
    return useShakeSensitivity ? SHAKE_THRESHOLD : SLEEP_THRESHOLD;
}

bool checkMotionAboveThreshold(float deltaX, float deltaY, float deltaZ, float threshold)
{
    return (deltaX > threshold) || (deltaY > threshold) || (deltaZ > threshold);
}

void updateMotionFlags(float deltaX, float deltaY, float deltaZ)
{
    lastAccelDeltaX = deltaX;
    lastAccelDeltaY = deltaY;
    lastAccelDeltaZ = deltaZ;
    
    lastMotionAboveShake = checkMotionAboveThreshold(deltaX, deltaY, deltaZ, SHAKE_THRESHOLD);
    lastMotionAboveSleep = checkMotionAboveThreshold(deltaX, deltaY, deltaZ, SLEEP_THRESHOLD);
}

float calculateAccelDelta(float current, float previous)
{
    return fabs(current - previous);
}

void setUp(void)
{
    // Reset state before each test
    useShakeSensitivity = false;
    lastAccelDeltaX = 0.0f;
    lastAccelDeltaY = 0.0f;
    lastAccelDeltaZ = 0.0f;
    lastMotionAboveShake = false;
    lastMotionAboveSleep = false;
}

void tearDown(void)
{
    // Clean up after each test
}

void test_threshold_constants(void)
{
    // Verify threshold constants are defined correctly
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, SLEEP_THRESHOLD);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, SHAKE_THRESHOLD);
}

void test_getCurrentThreshold_shake_mode(void)
{
    useShakeSensitivity = true;
    float threshold = getCurrentThreshold();
    TEST_ASSERT_FLOAT_WITHIN(0.001f, SHAKE_THRESHOLD, threshold);
}

void test_getCurrentThreshold_sleep_mode(void)
{
    useShakeSensitivity = false;
    float threshold = getCurrentThreshold();
    TEST_ASSERT_FLOAT_WITHIN(0.001f, SLEEP_THRESHOLD, threshold);
}

void test_calculateAccelDelta_positive_difference(void)
{
    float delta = calculateAccelDelta(5.0f, 2.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.0f, delta);
}

void test_calculateAccelDelta_negative_difference(void)
{
    float delta = calculateAccelDelta(2.0f, 5.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.0f, delta);
}

void test_calculateAccelDelta_zero_difference(void)
{
    float delta = calculateAccelDelta(5.0f, 5.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, delta);
}

void test_checkMotionAboveThreshold_all_below(void)
{
    bool result = checkMotionAboveThreshold(0.5f, 0.5f, 0.5f, 1.0f);
    TEST_ASSERT_FALSE(result);
}

void test_checkMotionAboveThreshold_x_above(void)
{
    bool result = checkMotionAboveThreshold(1.5f, 0.5f, 0.5f, 1.0f);
    TEST_ASSERT_TRUE(result);
}

void test_checkMotionAboveThreshold_y_above(void)
{
    bool result = checkMotionAboveThreshold(0.5f, 1.5f, 0.5f, 1.0f);
    TEST_ASSERT_TRUE(result);
}

void test_checkMotionAboveThreshold_z_above(void)
{
    bool result = checkMotionAboveThreshold(0.5f, 0.5f, 1.5f, 1.0f);
    TEST_ASSERT_TRUE(result);
}

void test_checkMotionAboveThreshold_all_above(void)
{
    bool result = checkMotionAboveThreshold(1.5f, 1.5f, 1.5f, 1.0f);
    TEST_ASSERT_TRUE(result);
}

void test_checkMotionAboveThreshold_exactly_at_threshold(void)
{
    // At threshold should be false (need to be ABOVE)
    bool result = checkMotionAboveThreshold(1.0f, 1.0f, 1.0f, 1.0f);
    TEST_ASSERT_FALSE(result);
}

void test_updateMotionFlags_no_motion(void)
{
    updateMotionFlags(0.5f, 0.5f, 0.5f);
    
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, lastAccelDeltaX);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, lastAccelDeltaY);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, lastAccelDeltaZ);
    TEST_ASSERT_FALSE(lastMotionAboveShake);
    TEST_ASSERT_FALSE(lastMotionAboveSleep);
}

void test_updateMotionFlags_shake_detected(void)
{
    updateMotionFlags(2.0f, 0.5f, 0.5f);
    
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.0f, lastAccelDeltaX);
    TEST_ASSERT_TRUE(lastMotionAboveShake);
    TEST_ASSERT_TRUE(lastMotionAboveSleep);
}

void test_updateMotionFlags_sleep_threshold_motion(void)
{
    // Motion that's above sleep threshold but not above shake (since they're equal)
    // This tests the boundary case
    updateMotionFlags(1.1f, 0.5f, 0.5f);
    
    TEST_ASSERT_TRUE(lastMotionAboveShake);
    TEST_ASSERT_TRUE(lastMotionAboveSleep);
}

void test_motion_detection_multiple_axes(void)
{
    // Test motion on multiple axes
    updateMotionFlags(1.5f, 1.2f, 0.8f);
    
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.5f, lastAccelDeltaX);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.2f, lastAccelDeltaY);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.8f, lastAccelDeltaZ);
    TEST_ASSERT_TRUE(lastMotionAboveShake);
}

void test_motion_detection_precision(void)
{
    // Test with very small values near threshold
    updateMotionFlags(1.01f, 0.99f, 0.5f);
    
    TEST_ASSERT_TRUE(lastMotionAboveShake);  // 1.01 > 1.0
    TEST_ASSERT_TRUE(lastMotionAboveSleep);
}

void test_motion_detection_zero_values(void)
{
    updateMotionFlags(0.0f, 0.0f, 0.0f);
    
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, lastAccelDeltaX);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, lastAccelDeltaY);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, lastAccelDeltaZ);
    TEST_ASSERT_FALSE(lastMotionAboveShake);
    TEST_ASSERT_FALSE(lastMotionAboveSleep);
}

void test_threshold_switching(void)
{
    // Test switching between shake and sleep thresholds
    useShakeSensitivity = true;
    TEST_ASSERT_FLOAT_WITHIN(0.001f, SHAKE_THRESHOLD, getCurrentThreshold());
    
    useShakeSensitivity = false;
    TEST_ASSERT_FLOAT_WITHIN(0.001f, SLEEP_THRESHOLD, getCurrentThreshold());
    
    useShakeSensitivity = true;
    TEST_ASSERT_FLOAT_WITHIN(0.001f, SHAKE_THRESHOLD, getCurrentThreshold());
}

void setup()
{
    UNITY_BEGIN();
    
    RUN_TEST(test_threshold_constants);
    RUN_TEST(test_getCurrentThreshold_shake_mode);
    RUN_TEST(test_getCurrentThreshold_sleep_mode);
    RUN_TEST(test_calculateAccelDelta_positive_difference);
    RUN_TEST(test_calculateAccelDelta_negative_difference);
    RUN_TEST(test_calculateAccelDelta_zero_difference);
    RUN_TEST(test_checkMotionAboveThreshold_all_below);
    RUN_TEST(test_checkMotionAboveThreshold_x_above);
    RUN_TEST(test_checkMotionAboveThreshold_y_above);
    RUN_TEST(test_checkMotionAboveThreshold_z_above);
    RUN_TEST(test_checkMotionAboveThreshold_all_above);
    RUN_TEST(test_checkMotionAboveThreshold_exactly_at_threshold);
    RUN_TEST(test_updateMotionFlags_no_motion);
    RUN_TEST(test_updateMotionFlags_shake_detected);
    RUN_TEST(test_updateMotionFlags_sleep_threshold_motion);
    RUN_TEST(test_motion_detection_multiple_axes);
    RUN_TEST(test_motion_detection_precision);
    RUN_TEST(test_motion_detection_zero_values);
    RUN_TEST(test_threshold_switching);
    
    UNITY_END();
}

void loop()
{
    // Unity tests run once
}
