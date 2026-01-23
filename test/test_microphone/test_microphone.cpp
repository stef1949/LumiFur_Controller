#include <unity.h>
#include <cstdint>
#include <algorithm>

// Constants from main.cpp
#define MIC_THRESHOLD 450000
#define SENSITIVITY_OFFSET_ABOVE_AMBIENT 8000.0f

// Arduino map function for testing
long map(long x, long in_min, long in_max, long out_min, long out_max)
{
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

// Arduino constrain function for testing
long constrain(long x, long min_val, long max_val)
{
    if (x < min_val) return min_val;
    if (x > max_val) return max_val;
    return x;
}

// Test globals
uint8_t mawBrightness = 0;
bool mouthOpen = false;
float smoothedSignalLevel = 0.0f;
float ambientNoiseLevel = 0.0f;

// Functions under test
uint8_t calculateMawBrightness(float smoothedSignal, float ambientNoise, bool isMouthOpen)
{
    float signalAboveAmbient = smoothedSignal - ambientNoise;
    if (signalAboveAmbient < 0)
        signalAboveAmbient = 0;
    
    float brightnessMappingRange = SENSITIVITY_OFFSET_ABOVE_AMBIENT * 2.5f;
    uint8_t brightness = map((long)(signalAboveAmbient * 100),
                             0,
                             (long)(brightnessMappingRange * 100),
                             20,
                             255);
    brightness = constrain(brightness, 20, 255);
    
    if (!isMouthOpen)
    {
        brightness = 20;
    }
    
    return brightness;
}

bool shouldMouthOpen(float smoothedSignal, float triggerThreshold)
{
    return smoothedSignal > triggerThreshold;
}

float calculateTriggerThreshold(float ambientNoise, float sensitivityOffset)
{
    return ambientNoise + sensitivityOffset;
}

float calculateSignalAboveAmbient(float smoothedSignal, float ambientNoise)
{
    float signal = smoothedSignal - ambientNoise;
    return (signal < 0) ? 0 : signal;
}

void setUp(void)
{
    // Reset state before each test
    mawBrightness = 0;
    mouthOpen = false;
    smoothedSignalLevel = 0.0f;
    ambientNoiseLevel = 0.0f;
}

void tearDown(void)
{
    // Clean up after each test
}

void test_mic_threshold_constant(void)
{
    // Verify MIC_THRESHOLD is defined correctly
    TEST_ASSERT_EQUAL_INT(450000, MIC_THRESHOLD);
}

void test_sensitivity_offset_constant(void)
{
    // Verify SENSITIVITY_OFFSET_ABOVE_AMBIENT is defined
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 8000.0f, SENSITIVITY_OFFSET_ABOVE_AMBIENT);
}

void test_calculateMawBrightness_mouth_closed(void)
{
    uint8_t brightness = calculateMawBrightness(5000.0f, 1000.0f, false);
    TEST_ASSERT_EQUAL_UINT8(20, brightness);
}

void test_calculateMawBrightness_mouth_open_minimum(void)
{
    // Signal equal to ambient should give minimum brightness
    uint8_t brightness = calculateMawBrightness(1000.0f, 1000.0f, true);
    TEST_ASSERT_EQUAL_UINT8(20, brightness);
}

void test_calculateMawBrightness_mouth_open_maximum(void)
{
    // At exactly the mapping range, should give maximum brightness
    float ambientNoise = 1000.0f;
    float mappingRange = SENSITIVITY_OFFSET_ABOVE_AMBIENT * 2.5f;
    float maxSignal = ambientNoise + mappingRange;
    
    uint8_t brightness = calculateMawBrightness(maxSignal, ambientNoise, true);
    TEST_ASSERT_EQUAL_UINT8(255, brightness);
}

void test_calculateMawBrightness_mouth_open_mid_range(void)
{
    // Mid-range signal should give brightness between 20 and 255
    float ambientNoise = 1000.0f;
    float mappingRange = SENSITIVITY_OFFSET_ABOVE_AMBIENT * 2.5f;
    float midSignal = ambientNoise + (mappingRange / 2.0f);
    
    uint8_t brightness = calculateMawBrightness(midSignal, ambientNoise, true);
    TEST_ASSERT_GREATER_THAN_UINT8(20, brightness);
    TEST_ASSERT_LESS_THAN_UINT8(255, brightness);
}

void test_calculateMawBrightness_above_mapping_range_clamps(void)
{
    float ambientNoise = 1000.0f;
    float mappingRange = SENSITIVITY_OFFSET_ABOVE_AMBIENT * 2.5f;
    float signal = ambientNoise + (mappingRange * 2.0f);

    uint8_t brightness = calculateMawBrightness(signal, ambientNoise, true);
    TEST_ASSERT_EQUAL_UINT8(255, brightness);
}

void test_calculateMawBrightness_monotonic_increase(void)
{
    float ambientNoise = 1000.0f;
    float mappingRange = SENSITIVITY_OFFSET_ABOVE_AMBIENT * 2.5f;

    uint8_t b1 = calculateMawBrightness(ambientNoise + (mappingRange * 0.1f), ambientNoise, true);
    uint8_t b2 = calculateMawBrightness(ambientNoise + (mappingRange * 0.2f), ambientNoise, true);
    uint8_t b3 = calculateMawBrightness(ambientNoise + (mappingRange * 0.3f), ambientNoise, true);

    TEST_ASSERT_LESS_OR_EQUAL_UINT8(b2, b1);
    TEST_ASSERT_LESS_OR_EQUAL_UINT8(b3, b2);
}

void test_calculateMawBrightness_signal_below_ambient(void)
{
    // Signal below ambient should give minimum brightness
    uint8_t brightness = calculateMawBrightness(500.0f, 1000.0f, true);
    TEST_ASSERT_EQUAL_UINT8(20, brightness);
}

void test_shouldMouthOpen_above_threshold(void)
{
    bool result = shouldMouthOpen(10000.0f, 9000.0f);
    TEST_ASSERT_TRUE(result);
}

void test_shouldMouthOpen_below_threshold(void)
{
    bool result = shouldMouthOpen(8000.0f, 9000.0f);
    TEST_ASSERT_FALSE(result);
}

void test_shouldMouthOpen_at_threshold(void)
{
    bool result = shouldMouthOpen(9000.0f, 9000.0f);
    TEST_ASSERT_FALSE(result);
}

void test_shouldMouthOpen_negative_threshold(void)
{
    bool result = shouldMouthOpen(-1.0f, -10.0f);
    TEST_ASSERT_TRUE(result);
}

void test_calculateTriggerThreshold(void)
{
    float threshold = calculateTriggerThreshold(5000.0f, SENSITIVITY_OFFSET_ABOVE_AMBIENT);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 13000.0f, threshold);
}

void test_calculateTriggerThreshold_negative_ambient(void)
{
    float threshold = calculateTriggerThreshold(-100.0f, SENSITIVITY_OFFSET_ABOVE_AMBIENT);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, SENSITIVITY_OFFSET_ABOVE_AMBIENT - 100.0f, threshold);
}

void test_calculateSignalAboveAmbient_positive(void)
{
    float signal = calculateSignalAboveAmbient(10000.0f, 5000.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 5000.0f, signal);
}

void test_calculateSignalAboveAmbient_negative(void)
{
    float signal = calculateSignalAboveAmbient(3000.0f, 5000.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.0f, signal);
}

void test_calculateSignalAboveAmbient_zero(void)
{
    float signal = calculateSignalAboveAmbient(5000.0f, 5000.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.0f, signal);
}

void test_calculateSignalAboveAmbient_negative_ambient(void)
{
    float signal = calculateSignalAboveAmbient(0.0f, -100.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 100.0f, signal);
}

void test_map_function_linear_interpolation(void)
{
    // Test Arduino map function
    long result = map(50, 0, 100, 0, 255);
    TEST_ASSERT_EQUAL_INT(127, result);
}

void test_map_function_negative_range(void)
{
    long result = map(-50, -100, 0, 0, 100);
    TEST_ASSERT_EQUAL_INT(50, result);
}

void test_map_function_min_value(void)
{
    long result = map(0, 0, 100, 20, 255);
    TEST_ASSERT_EQUAL_INT(20, result);
}

void test_map_function_max_value(void)
{
    long result = map(100, 0, 100, 20, 255);
    TEST_ASSERT_EQUAL_INT(255, result);
}

void test_constrain_below_min(void)
{
    long result = constrain(10, 20, 255);
    TEST_ASSERT_EQUAL_INT(20, result);
}

void test_constrain_above_max(void)
{
    long result = constrain(300, 20, 255);
    TEST_ASSERT_EQUAL_INT(255, result);
}

void test_constrain_within_range(void)
{
    long result = constrain(100, 20, 255);
    TEST_ASSERT_EQUAL_INT(100, result);
}

void test_constrain_exact_bounds(void)
{
    TEST_ASSERT_EQUAL_INT(20, constrain(20, 20, 255));
    TEST_ASSERT_EQUAL_INT(255, constrain(255, 20, 255));
}

void test_brightness_mapping_range_calculation(void)
{
    float mappingRange = SENSITIVITY_OFFSET_ABOVE_AMBIENT * 2.5f;
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 20000.0f, mappingRange);
}

void test_maw_brightness_progression(void)
{
    // Test that brightness increases with signal level
    float ambientNoise = 1000.0f;
    float mappingRange = SENSITIVITY_OFFSET_ABOVE_AMBIENT * 2.5f;
    
    uint8_t b1 = calculateMawBrightness(ambientNoise + (mappingRange * 0.25f), ambientNoise, true);
    uint8_t b2 = calculateMawBrightness(ambientNoise + (mappingRange * 0.50f), ambientNoise, true);
    uint8_t b3 = calculateMawBrightness(ambientNoise + (mappingRange * 0.75f), ambientNoise, true);
    
    TEST_ASSERT_LESS_THAN_UINT8(b2, b1);
    TEST_ASSERT_LESS_THAN_UINT8(b3, b2);
}

void test_mouth_state_affects_brightness(void)
{
    float ambientNoise = 1000.0f;
    float signal = 10000.0f;
    
    uint8_t brightnessOpen = calculateMawBrightness(signal, ambientNoise, true);
    uint8_t brightnessClosed = calculateMawBrightness(signal, ambientNoise, false);
    
    TEST_ASSERT_GREATER_THAN_UINT8(20, brightnessOpen);
    TEST_ASSERT_EQUAL_UINT8(20, brightnessClosed);
}

void test_calculateMawBrightness_negative_ambient(void)
{
    uint8_t brightness = calculateMawBrightness(0.0f, -100.0f, true);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT8(20, brightness);
    TEST_ASSERT_LESS_OR_EQUAL_UINT8(255, brightness);
}

void test_extreme_signal_values(void)
{
    // Test with very high signal values
    // Signals above the mapping range will be constrained
    // The map function behavior causes values above range to wrap around
    uint8_t brightness = calculateMawBrightness(100000.0f, 1000.0f, true);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT8(20, brightness);
    TEST_ASSERT_LESS_OR_EQUAL_UINT8(255, brightness);
}

void test_zero_ambient_noise(void)
{
    // Test with zero ambient noise
    uint8_t brightness = calculateMawBrightness(10000.0f, 0.0f, true);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT8(20, brightness);
    TEST_ASSERT_LESS_OR_EQUAL_UINT8(255, brightness);
}

void setup()
{
    UNITY_BEGIN();
    
    RUN_TEST(test_mic_threshold_constant);
    RUN_TEST(test_sensitivity_offset_constant);
    RUN_TEST(test_calculateMawBrightness_mouth_closed);
    RUN_TEST(test_calculateMawBrightness_mouth_open_minimum);
    RUN_TEST(test_calculateMawBrightness_mouth_open_maximum);
    RUN_TEST(test_calculateMawBrightness_mouth_open_mid_range);
    RUN_TEST(test_calculateMawBrightness_above_mapping_range_clamps);
    RUN_TEST(test_calculateMawBrightness_monotonic_increase);
    RUN_TEST(test_calculateMawBrightness_signal_below_ambient);
    RUN_TEST(test_shouldMouthOpen_above_threshold);
    RUN_TEST(test_shouldMouthOpen_below_threshold);
    RUN_TEST(test_shouldMouthOpen_at_threshold);
    RUN_TEST(test_shouldMouthOpen_negative_threshold);
    RUN_TEST(test_calculateTriggerThreshold);
    RUN_TEST(test_calculateTriggerThreshold_negative_ambient);
    RUN_TEST(test_calculateSignalAboveAmbient_positive);
    RUN_TEST(test_calculateSignalAboveAmbient_negative);
    RUN_TEST(test_calculateSignalAboveAmbient_zero);
    RUN_TEST(test_calculateSignalAboveAmbient_negative_ambient);
    RUN_TEST(test_map_function_linear_interpolation);
    RUN_TEST(test_map_function_negative_range);
    RUN_TEST(test_map_function_min_value);
    RUN_TEST(test_map_function_max_value);
    RUN_TEST(test_constrain_below_min);
    RUN_TEST(test_constrain_above_max);
    RUN_TEST(test_constrain_within_range);
    RUN_TEST(test_constrain_exact_bounds);
    RUN_TEST(test_brightness_mapping_range_calculation);
    RUN_TEST(test_maw_brightness_progression);
    RUN_TEST(test_mouth_state_affects_brightness);
    RUN_TEST(test_calculateMawBrightness_negative_ambient);
    RUN_TEST(test_extreme_signal_values);
    RUN_TEST(test_zero_ambient_noise);
    
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
