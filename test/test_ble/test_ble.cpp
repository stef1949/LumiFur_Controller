#include <unity.h>

#include "core/TemperatureHistory.h"
#include <climits>
#include <limits>
using namespace telemetry;



void setUp() {}

void tearDown() {}

void test_empty_history()
{
  TemperatureHistory h; auto s=h.snapshot(); TEST_ASSERT_EQUAL_UINT(0,s.count); TEST_ASSERT_EQUAL_UINT(0,historyChunkCount(s));
}

void test_fixed10_conversion()
{
  TEST_ASSERT_EQUAL_INT16(253,temperatureToFixed10(25.3f)); TEST_ASSERT_EQUAL_INT16(-105,temperatureToFixed10(-10.5f));
}

void test_fixed10_clamps_before_rounding()
{
  TEST_ASSERT_EQUAL_INT16(INT16_MAX,temperatureToFixed10(1e30f)); TEST_ASSERT_EQUAL_INT16(INT16_MIN,temperatureToFixed10(-1e30f));
}

void test_nonfinite_temperature()
{
  TEST_ASSERT_EQUAL_INT16(0,temperatureToFixed10(std::numeric_limits<float>::quiet_NaN()));
}

void test_history_is_chronological_after_multiple_wraps()
{
  TemperatureHistory h; for(int i=0;i<113;++i)h.append(i,i*1000); auto s=h.snapshot(); TEST_ASSERT_EQUAL_UINT(50,s.count); for(int i=0;i<50;++i)TEST_ASSERT_EQUAL_INT16(i+63,s.samples[i]);
}

void test_clear_after_wrap()
{
  TemperatureHistory h; for(int i=0;i<60;++i)h.append(i,i); h.clear(); h.clear(); auto s=h.snapshot(); TEST_ASSERT_EQUAL_UINT(0,s.count); h.append(-22,999); s=h.snapshot(); TEST_ASSERT_EQUAL_INT16(-22,s.samples[0]); TEST_ASSERT_EQUAL_UINT(999,s.newestSampleMs);
}

void test_snapshot_does_not_change_when_history_is_updated()
{
  TemperatureHistory h; h.append(250,1000); auto s=h.snapshot(); h.append(300,2000); h.clear(); TEST_ASSERT_EQUAL_UINT(1,s.count); TEST_ASSERT_EQUAL_INT16(250,s.samples[0]);
}

void test_start_packet_uses_sampling_interval()
{
  TemperatureHistory h; h.append(250,1000); auto s=h.snapshot(); uint8_t b[8]; TEST_ASSERT_EQUAL_UINT(8,encodeHistoryStart(s,1000,b,8)); const uint8_t expected[]={0xA0,1,kTemperatureIntervalSeconds,0,1,0,0,0}; TEST_ASSERT_EQUAL_UINT8_ARRAY(expected,b,8); TEST_ASSERT_EQUAL_UINT(1000,kTemperatureIntervalMs);
}

void test_start_packet_includes_age_since_last_sample()
{
  TemperatureHistory h; h.append(250,1000); uint8_t b[8]; encodeHistoryStart(h.snapshot(),121000,b,8); TEST_ASSERT_EQUAL_UINT8(2,b[6]);
}

void test_start_packet_age_across_clock_rollover()
{
  TemperatureHistory h; h.append(250,UINT32_MAX-999); uint8_t b[8]; encodeHistoryStart(h.snapshot(),59000,b,8); TEST_ASSERT_EQUAL_UINT8(1,b[6]);
}

void test_empty_history_has_start_packet()
{
  TemperatureHistory h; uint8_t b[8]; TEST_ASSERT_EQUAL_UINT(8,encodeHistoryStart(h.snapshot(),5000,b,8)); TEST_ASSERT_EQUAL_UINT8(0,b[4]);
}

void test_chunk_fits_default_mtu()
{
  TemperatureHistory h; for(int i=0;i<50;++i)h.append(i,1000*i); auto s=h.snapshot(); uint8_t b[kHistoryPacketMaxBytes]; TEST_ASSERT_EQUAL_UINT(7,historyChunkCount(s)); TEST_ASSERT_EQUAL_UINT(20,encodeHistoryChunk(s,0,b,sizeof(b))); TEST_ASSERT_EQUAL_UINT8(8,b[3]); TEST_ASSERT_EQUAL_UINT(8,encodeHistoryChunk(s,6,b,sizeof(b))); TEST_ASSERT_EQUAL_UINT8(2,b[3]);
}

void test_chunk_signed_little_endian_encoding()
{
  TemperatureHistory h; h.append(-105,0); h.append(253,1000); uint8_t b[8]; encodeHistoryChunk(h.snapshot(),0,b,sizeof(b)); const uint8_t expected[]={0xA1,0,1,2,0x97,0xFF,0xFD,0}; TEST_ASSERT_EQUAL_UINT8_ARRAY(expected,b,8);
}

void test_bad_chunk_and_short_output_are_rejected()
{
  TemperatureHistory h; h.append(25,0); auto s=h.snapshot(); uint8_t b[8]={99}; TEST_ASSERT_EQUAL_UINT(0,encodeHistoryChunk(s,1,b,8)); TEST_ASSERT_EQUAL_UINT(0,encodeHistoryChunk(s,0,b,5)); TEST_ASSERT_EQUAL_UINT(0,encodeHistoryChunk(s,0,nullptr,8)); TEST_ASSERT_EQUAL_UINT8(99,b[0]); TEST_ASSERT_EQUAL_UINT(0,encodeHistoryStart(s,0,b,7));
}

void test_invalid_snapshot_count_is_rejected()
{
  TemperatureSnapshot s; s.count=kHistoryCapacity+1; uint8_t b[20]; TEST_ASSERT_EQUAL_UINT(0,encodeHistoryStart(s,0,b,20)); TEST_ASSERT_EQUAL_UINT(0,encodeHistoryChunk(s,0,b,20));
}

int main()
{
  UNITY_BEGIN();

  RUN_TEST(test_empty_history);

  RUN_TEST(test_fixed10_conversion);

  RUN_TEST(test_fixed10_clamps_before_rounding);

  RUN_TEST(test_nonfinite_temperature);

  RUN_TEST(test_history_is_chronological_after_multiple_wraps);

  RUN_TEST(test_clear_after_wrap);

  RUN_TEST(test_snapshot_does_not_change_when_history_is_updated);

  RUN_TEST(test_start_packet_uses_sampling_interval);

  RUN_TEST(test_start_packet_includes_age_since_last_sample);

  RUN_TEST(test_start_packet_age_across_clock_rollover);

  RUN_TEST(test_empty_history_has_start_packet);

  RUN_TEST(test_chunk_fits_default_mtu);

  RUN_TEST(test_chunk_signed_little_endian_encoding);

  RUN_TEST(test_bad_chunk_and_short_output_are_rejected);

  RUN_TEST(test_invalid_snapshot_count_is_rejected);

  return UNITY_END();
}
