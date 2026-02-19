/**
 * Embedded unit tests for StringView (runs on ESP32: pio test -e esp32-s3-devkitc-1-16m)
 */
#include <unity.h>
#include <Arduino.h>
#include "commons/StringView.h"

void setUp(void) {}
void tearDown(void) {}

void test_stringview_from_cstr(void) {
    const char* str = "hello";
    StringView sv(str, 5);
    TEST_ASSERT_EQUAL_PTR(str, sv.data());
    TEST_ASSERT_EQUAL(5, sv.size());
    TEST_ASSERT_FALSE(sv.empty());
}

void test_stringview_from_string(void) {
    String s = "world";
    StringView sv(s.c_str(), s.length());
    TEST_ASSERT_EQUAL(s.length(), sv.size());
    TEST_ASSERT_EQUAL_STRING_LEN("world", sv.data(), 5);
}

void test_stringview_empty(void) {
    StringView sv("", 0);
    TEST_ASSERT_EQUAL(0, sv.size());
    TEST_ASSERT_TRUE(sv.empty());
}

void setup() {
    delay(2000);
    UNITY_BEGIN();
    RUN_TEST(test_stringview_from_cstr);
    RUN_TEST(test_stringview_from_string);
    RUN_TEST(test_stringview_empty);
    UNITY_END();
}

void loop() {}
