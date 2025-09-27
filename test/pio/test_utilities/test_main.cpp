#include <Arduino.h>
#include <unity.h>

#include <FixedPoints.h>
#include <FixedPointsCommon.h>

#include "utilities.h"

// Ensure we don't inline away functions under test
#if defined(ARDUINO)
#define TEST_IRAM_ATTR IRAM_ATTR
#else
#define TEST_IRAM_ATTR
#endif

void setUp() {}
void tearDown() {}

void test_fminmax_fixed() {
    SQ15x16 a = SQ15x16(0.25);
    SQ15x16 b = SQ15x16(0.75);
    TEST_ASSERT_TRUE(fmin_fixed(a,b) == a);
    TEST_ASSERT_TRUE(fmax_fixed(a,b) == b);
}

void test_fmod_fixed_basic() {
    SQ15x16 x = SQ15x16(5.5);
    SQ15x16 d = SQ15x16(2.0);
    SQ15x16 r = fmod_fixed(x, d);
    TEST_ASSERT_FLOAT_WITHIN(1e-3, 1.5, double(r));
}

void test_low_pass_filter_float() {
    float last = 0.0f;
    float out = low_pass_filter(/*new_data*/1.0f, last, /*sample_rate*/1000, /*cutoff*/10.0f);
    TEST_ASSERT_TRUE(out > 0.0f);
    TEST_ASSERT_TRUE(out < 1.0f);
}

void test_bytes_to_hex() {
    byte in[4] = {0x01, 0xAB, 0x00, 0xFF};
    char buf[9] = {};
    bytes_to_hex_string(in, buf);
    TEST_ASSERT_EQUAL_STRING("01AB00FF", buf);
}

void setup() {
    delay(200);
    UNITY_BEGIN();
    RUN_TEST(test_fminmax_fixed);
    RUN_TEST(test_fmod_fixed_basic);
    RUN_TEST(test_low_pass_filter_float);
    RUN_TEST(test_bytes_to_hex);
    UNITY_END();
}

void loop() {}

