#include <unity.h>

#include "core/InputState.h"
using namespace input;



void setUp() {}

void tearDown() {}

void test_bounce_is_filtered()
{
  ButtonState b; updateButtonState(b,true,100,50); updateButtonState(b,false,120,50); updateButtonState(b,true,130,50);updateButtonState(b,true,179,50);TEST_ASSERT_FALSE(b.stablePressed);updateButtonState(b,true,180,50);TEST_ASSERT_TRUE(b.stablePressed);
}

void test_release_produces_one_short_press()
{
  ButtonState b;updateButtonState(b,true,0,50);updateButtonState(b,true,50,50);updateButtonState(b,false,100,50);updateButtonState(b,false,150,50);TEST_ASSERT_TRUE(consumeButtonShortPress(b));TEST_ASSERT_FALSE(consumeButtonShortPress(b));
}

void test_long_press_fires_once_and_suppresses_short()
{
  ButtonState b;updateButtonState(b,true,0,50);updateButtonState(b,true,50,50);TEST_ASSERT_FALSE(consumeButtonLongPress(b,3049,3000));TEST_ASSERT_TRUE(consumeButtonLongPress(b,3050,3000));TEST_ASSERT_FALSE(consumeButtonLongPress(b,4000,3000));updateButtonState(b,false,4100,50);updateButtonState(b,false,4150,50);TEST_ASSERT_FALSE(consumeButtonShortPress(b));
}

void test_pairing_chord_suppresses_both_buttons()
{
  ButtonState a,b; a.stablePressed=b.stablePressed=true;suppressButtonPress(a);suppressButtonPress(b);updateButtonState(a,false,100,0);updateButtonState(b,false,100,0);TEST_ASSERT_FALSE(consumeButtonShortPress(a));TEST_ASSERT_FALSE(consumeButtonShortPress(b));
}

void test_debounce_across_rollover()
{
  ButtonState b;updateButtonState(b,true,UINT32_MAX-20,50);updateButtonState(b,true,29,50);TEST_ASSERT_TRUE(b.stablePressed);
}

int main()
{
  UNITY_BEGIN();

  RUN_TEST(test_bounce_is_filtered);

  RUN_TEST(test_release_produces_one_short_press);

  RUN_TEST(test_long_press_fires_once_and_suppresses_short);

  RUN_TEST(test_pairing_chord_suppresses_both_buttons);

  RUN_TEST(test_debounce_across_rollover);

  return UNITY_END();
}
