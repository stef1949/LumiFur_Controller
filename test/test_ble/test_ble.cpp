#include <unity.h>
#include <cstring>
#include <cstdint>

// Mock definitions for BLE testing
#define HISTORY_BUFFER_SIZE 50

// Test globals - simulating BLE module state
float temperatureHistory[HISTORY_BUFFER_SIZE];
int historyWriteIndex = 0;
int validHistoryCount = 0;
bool bufferFull = false;

// Mock packet structure constants
const uint8_t PKT_TYPE_HISTORY = 0x01;
const uint8_t MAX_FLOATS_PER_CHUNK = 4;

// Function under test - temperature history management
void storeTemperatureInHistory(float temp)
{
    temperatureHistory[historyWriteIndex] = temp;
    historyWriteIndex++;
    if (!bufferFull)
    {
        validHistoryCount++;
    }
    if (historyWriteIndex >= HISTORY_BUFFER_SIZE)
    {
        historyWriteIndex = 0; // Wrap around
        bufferFull = true;
    }
}

void clearHistoryBuffer()
{
    memset(temperatureHistory, 0, sizeof(temperatureHistory));
    historyWriteIndex = 0;
    validHistoryCount = 0;
    bufferFull = false;
}

// Helper function to clamp scroll speed
uint8_t clampScrollSpeed(uint8_t speed)
{
    if (speed < 1)
        return 1;
    if (speed > 100)
        return 100;
    return speed;
}

void setUp(void)
{
    // Reset history buffer before each test
    clearHistoryBuffer();
}

void tearDown(void)
{
    // Clean up after each test
}

void test_temperature_history_initialization(void)
{
    TEST_ASSERT_EQUAL_INT(0, historyWriteIndex);
    TEST_ASSERT_EQUAL_INT(0, validHistoryCount);
    TEST_ASSERT_FALSE(bufferFull);
}

void test_temperature_history_single_entry(void)
{
    float temp = 25.5f;
    storeTemperatureInHistory(temp);
    
    TEST_ASSERT_EQUAL_INT(1, historyWriteIndex);
    TEST_ASSERT_EQUAL_INT(1, validHistoryCount);
    TEST_ASSERT_FALSE(bufferFull);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, temp, temperatureHistory[0]);
}

void test_temperature_history_multiple_entries(void)
{
    float temps[] = {20.0f, 21.5f, 22.3f, 23.8f, 24.2f};
    int numTemps = sizeof(temps) / sizeof(temps[0]);
    
    for (int i = 0; i < numTemps; i++)
    {
        storeTemperatureInHistory(temps[i]);
    }
    
    TEST_ASSERT_EQUAL_INT(numTemps, historyWriteIndex);
    TEST_ASSERT_EQUAL_INT(numTemps, validHistoryCount);
    TEST_ASSERT_FALSE(bufferFull);
    
    // Verify all temperatures were stored correctly
    for (int i = 0; i < numTemps; i++)
    {
        TEST_ASSERT_FLOAT_WITHIN(0.01f, temps[i], temperatureHistory[i]);
    }
}

void test_temperature_history_buffer_wraparound(void)
{
    // Fill the buffer completely
    for (int i = 0; i < HISTORY_BUFFER_SIZE; i++)
    {
        storeTemperatureInHistory((float)i);
    }
    
    TEST_ASSERT_EQUAL_INT(0, historyWriteIndex); // Should wrap to 0
    TEST_ASSERT_EQUAL_INT(HISTORY_BUFFER_SIZE, validHistoryCount);
    TEST_ASSERT_TRUE(bufferFull);
    
    // Add one more entry - should overwrite index 0
    float newTemp = 99.9f;
    storeTemperatureInHistory(newTemp);
    
    TEST_ASSERT_EQUAL_INT(1, historyWriteIndex); // Should be at index 1
    TEST_ASSERT_EQUAL_INT(HISTORY_BUFFER_SIZE, validHistoryCount); // Count stays at max
    TEST_ASSERT_TRUE(bufferFull);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, newTemp, temperatureHistory[0]); // Verify overwrite
}

void test_temperature_history_circular_buffer_behavior(void)
{
    // Fill buffer to capacity
    for (int i = 0; i < HISTORY_BUFFER_SIZE; i++)
    {
        storeTemperatureInHistory((float)i);
    }
    
    // Add 10 more entries
    for (int i = 0; i < 10; i++)
    {
        storeTemperatureInHistory(100.0f + i);
    }
    
    TEST_ASSERT_EQUAL_INT(10, historyWriteIndex); // Should wrap around to index 10
    TEST_ASSERT_EQUAL_INT(HISTORY_BUFFER_SIZE, validHistoryCount);
    TEST_ASSERT_TRUE(bufferFull);
    
    // Verify the last 10 entries were written correctly
    for (int i = 0; i < 10; i++)
    {
        TEST_ASSERT_FLOAT_WITHIN(0.01f, 100.0f + i, temperatureHistory[i]);
    }
}

void test_clear_history_buffer(void)
{
    // Add some entries
    for (int i = 0; i < 10; i++)
    {
        storeTemperatureInHistory((float)i);
    }
    
    // Clear the buffer
    clearHistoryBuffer();
    
    TEST_ASSERT_EQUAL_INT(0, historyWriteIndex);
    TEST_ASSERT_EQUAL_INT(0, validHistoryCount);
    TEST_ASSERT_FALSE(bufferFull);
}

void test_scroll_speed_clamping_valid_range(void)
{
    // Test valid range values
    TEST_ASSERT_EQUAL_UINT8(1, clampScrollSpeed(1));
    TEST_ASSERT_EQUAL_UINT8(50, clampScrollSpeed(50));
    TEST_ASSERT_EQUAL_UINT8(100, clampScrollSpeed(100));
}

void test_scroll_speed_clamping_below_minimum(void)
{
    // Test values below minimum
    TEST_ASSERT_EQUAL_UINT8(1, clampScrollSpeed(0));
}

void test_scroll_speed_clamping_above_maximum(void)
{
    // Test values above maximum
    TEST_ASSERT_EQUAL_UINT8(100, clampScrollSpeed(101));
    TEST_ASSERT_EQUAL_UINT8(100, clampScrollSpeed(150));
    TEST_ASSERT_EQUAL_UINT8(100, clampScrollSpeed(255));
}

void test_ble_packet_constants(void)
{
    // Verify packet structure constants are correctly defined
    TEST_ASSERT_EQUAL_UINT8(0x01, PKT_TYPE_HISTORY);
    TEST_ASSERT_EQUAL_UINT8(4, MAX_FLOATS_PER_CHUNK);
}

void test_temperature_history_capacity(void)
{
    // Verify buffer size constant
    TEST_ASSERT_EQUAL_INT(50, HISTORY_BUFFER_SIZE);
    
    // Verify we can store exactly HISTORY_BUFFER_SIZE entries
    for (int i = 0; i < HISTORY_BUFFER_SIZE; i++)
    {
        storeTemperatureInHistory((float)i);
    }
    
    TEST_ASSERT_EQUAL_INT(HISTORY_BUFFER_SIZE, validHistoryCount);
}

void test_temperature_values_precision(void)
{
    // Test storing various temperature values with precision
    float testTemps[] = {-10.5f, 0.0f, 25.3f, 37.7f, 100.2f};
    
    for (size_t i = 0; i < sizeof(testTemps) / sizeof(testTemps[0]); i++)
    {
        storeTemperatureInHistory(testTemps[i]);
        TEST_ASSERT_FLOAT_WITHIN(0.01f, testTemps[i], temperatureHistory[i]);
    }
}

void test_chunk_calculation_logic(void)
{
    // Test chunk calculation for different history sizes
    // Formula: totalChunks = (validHistoryCount + MAX_FLOATS_PER_CHUNK - 1) / MAX_FLOATS_PER_CHUNK
    
    int testCases[][2] = {
        {0, 0},   // 0 entries -> 0 chunks
        {1, 1},   // 1 entry -> 1 chunk
        {4, 1},   // 4 entries -> 1 chunk
        {5, 2},   // 5 entries -> 2 chunks
        {8, 2},   // 8 entries -> 2 chunks
        {9, 3},   // 9 entries -> 3 chunks
        {50, 13}  // 50 entries -> 13 chunks
    };
    
    for (size_t i = 0; i < sizeof(testCases) / sizeof(testCases[0]); i++)
    {
        int entries = testCases[i][0];
        int expectedChunks = testCases[i][1];
        
        int calculatedChunks = (entries + MAX_FLOATS_PER_CHUNK - 1) / MAX_FLOATS_PER_CHUNK;
        TEST_ASSERT_EQUAL_INT(expectedChunks, calculatedChunks);
    }
}

void setup()
{
    UNITY_BEGIN();
    
    RUN_TEST(test_temperature_history_initialization);
    RUN_TEST(test_temperature_history_single_entry);
    RUN_TEST(test_temperature_history_multiple_entries);
    RUN_TEST(test_temperature_history_buffer_wraparound);
    RUN_TEST(test_temperature_history_circular_buffer_behavior);
    RUN_TEST(test_clear_history_buffer);
    RUN_TEST(test_scroll_speed_clamping_valid_range);
    RUN_TEST(test_scroll_speed_clamping_below_minimum);
    RUN_TEST(test_scroll_speed_clamping_above_maximum);
    RUN_TEST(test_ble_packet_constants);
    RUN_TEST(test_temperature_history_capacity);
    RUN_TEST(test_temperature_values_precision);
    RUN_TEST(test_chunk_calculation_logic);
    
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
