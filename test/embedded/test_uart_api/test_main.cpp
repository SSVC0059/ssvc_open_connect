/**
 * Тесты по спецификации SSVC0059_V2 UART API v1.7
 * https://github.com/SmartModule-ru/ssvc0059_v2_uart_api_docs/blob/v1.7/README.md
 */
#include <unity.h>
#include <Arduino.h>
#include "core/SsvcUartApiSpec/SsvcUartApiSpec.h"
#include <ArduinoJson.h>
#include <string>

void setUp(void) {}
void tearDown(void) {}

// === Тесты API констант ===

void test_api_speed_limits(void) {
    TEST_ASSERT_EQUAL_FLOAT(99.9f, SsvcUartApiSpec::SPEED_TIME_MAX);
    TEST_ASSERT_EQUAL(999, SsvcUartApiSpec::SPEED_PERIOD_MAX);
}

void test_api_hyst_limits(void) {
    TEST_ASSERT_EQUAL_FLOAT(0.0f, SsvcUartApiSpec::HYST_MIN);
    TEST_ASSERT_EQUAL_FLOAT(50.0f, SsvcUartApiSpec::HYST_MAX);
}

void test_api_decrement_limits(void) {
    TEST_ASSERT_EQUAL(0, SsvcUartApiSpec::DECREMENT_MIN);
    TEST_ASSERT_EQUAL(100, SsvcUartApiSpec::DECREMENT_MAX);
}

void test_api_tank_mmhg_limits(void) {
    TEST_ASSERT_EQUAL_FLOAT(0.0f, SsvcUartApiSpec::TANK_MMHG_MIN);
    TEST_ASSERT_EQUAL_FLOAT(50.0f, SsvcUartApiSpec::TANK_MMHG_MAX);
}

void test_api_heads_timer_max(void) {
    TEST_ASSERT_EQUAL(86400, SsvcUartApiSpec::HEADS_TIMER_MAX);
    TEST_ASSERT_EQUAL(300, SsvcUartApiSpec::HEADS_TIMER_STEP);
}

// === Тесты валидации speed (heads, hearts, late_heads, tails) ===

void test_isValidSpeed_valid(void) {
    TEST_ASSERT_TRUE(SsvcUartApiSpec::isValidSpeed(30.0f, 180));
    TEST_ASSERT_TRUE(SsvcUartApiSpec::isValidSpeed(99.0f, 100));
    TEST_ASSERT_TRUE(SsvcUartApiSpec::isValidSpeed(0.0f, 0));
}

void test_isValidSpeed_timeExceedsPeriod(void) {
    TEST_ASSERT_FALSE(SsvcUartApiSpec::isValidSpeed(100.0f, 99));
}

void test_isValidSpeed_timeOverMax(void) {
    TEST_ASSERT_FALSE(SsvcUartApiSpec::isValidSpeed(100.0f, 999));
}

void test_isValidSpeed_periodOverMax(void) {
    TEST_ASSERT_FALSE(SsvcUartApiSpec::isValidSpeed(50.0f, 1000));
}

// === Тесты clampSpeed ===

void test_clampSpeed_enforcesLimits(void) {
    float t = 150.0f;
    int p = 1500;
    SsvcUartApiSpec::clampSpeed(t, p);
    TEST_ASSERT_EQUAL_FLOAT(99.9f, t);
    TEST_ASSERT_EQUAL(999, p);
}

void test_clampSpeed_timeNotExceedPeriod(void) {
    float t = 80.0f;
    int p = 50;
    SsvcUartApiSpec::clampSpeed(t, p);
    TEST_ASSERT_EQUAL_FLOAT(50.0f, t);
    TEST_ASSERT_EQUAL(50, p);
}

// === Тесты hyst, decrement, tank_mmhg ===

void test_isValidHyst(void) {
    TEST_ASSERT_TRUE(SsvcUartApiSpec::isValidHyst(0.25f));
    TEST_ASSERT_TRUE(SsvcUartApiSpec::isValidHyst(50.0f));
    TEST_ASSERT_FALSE(SsvcUartApiSpec::isValidHyst(50.01f));
}

void test_isValidDecrement(void) {
    TEST_ASSERT_TRUE(SsvcUartApiSpec::isValidDecrement(20));
    TEST_ASSERT_TRUE(SsvcUartApiSpec::isValidDecrement(100));
    TEST_ASSERT_FALSE(SsvcUartApiSpec::isValidDecrement(101));
}

void test_isValidTankMmhg(void) {
    TEST_ASSERT_TRUE(SsvcUartApiSpec::isValidTankMmhg(30.0f));
    TEST_ASSERT_TRUE(SsvcUartApiSpec::isValidTankMmhg(50.0f));
    TEST_ASSERT_FALSE(SsvcUartApiSpec::isValidTankMmhg(51.0f));
}

void test_isValidHeadsTimer(void) {
    TEST_ASSERT_TRUE(SsvcUartApiSpec::isValidHeadsTimer(36000));
    TEST_ASSERT_TRUE(SsvcUartApiSpec::isValidHeadsTimer(900));
    TEST_ASSERT_FALSE(SsvcUartApiSpec::isValidHeadsTimer(301));
}

// === Тесты парсинга формата [time,period] ===

void test_parseSpeedParam(void) {
    float t;
    int p;
    TEST_ASSERT_TRUE(SsvcUartApiSpec::parseSpeedParam("[30.0,180]", t, p));
    TEST_ASSERT_EQUAL_FLOAT(30.0f, t);
    TEST_ASSERT_EQUAL(180, p);
}

void test_parseSpeedParam_heads_example(void) {
    float t;
    int p;
    TEST_ASSERT_TRUE(SsvcUartApiSpec::parseSpeedParam("[99.0,100]", t, p));
    TEST_ASSERT_EQUAL_FLOAT(99.0f, t);
    TEST_ASSERT_EQUAL(100, p);
}

// === Тесты формата ответа GET_SETTINGS (JSON) ===

void test_get_settings_response_structure(void) {
    const char* json = R"({
        "type": "response",
        "request": "GET_SETTINGS",
        "result": "OK",
        "settings": {
            "heads": [24.5, 100],
            "hearts": [2.5, 5],
            "hyst": 0.25,
            "decrement": 100,
            "formula": 1,
            "tank_mmhg": 10,
            "valve_bw": [1100, 1200, 1300]
        }
    })";
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, json);
    TEST_ASSERT_FALSE(err);

    TEST_ASSERT_EQUAL_STRING("response", doc["type"].as<const char*>());
    TEST_ASSERT_EQUAL_STRING("GET_SETTINGS", doc["request"].as<const char*>());
    TEST_ASSERT_EQUAL_STRING("OK", doc["result"].as<const char*>());

    JsonObject settings = doc["settings"];
    TEST_ASSERT_FALSE(settings.isNull());

    JsonArray heads = settings["heads"];
    TEST_ASSERT_EQUAL(2, heads.size());
    TEST_ASSERT_EQUAL_FLOAT(24.5f, heads[0].as<float>());
    TEST_ASSERT_EQUAL(100, heads[1].as<int>());

    TEST_ASSERT_EQUAL_FLOAT(0.25f, settings["hyst"].as<float>());
    TEST_ASSERT_EQUAL(100, settings["decrement"].as<int>());
}

// === Тесты формата ответа VERSION ===

void test_version_response_structure(void) {
    const char* json = R"({
        "type": "response",
        "request": "VERSION",
        "result": "OK",
        "manufacturer": "SmartModule",
        "model": "SSVC0059_V2",
        "version": "2.2.37",
        "api": "1.7"
    })";
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, json);
    TEST_ASSERT_FALSE(err);

    TEST_ASSERT_EQUAL_STRING("response", doc["type"].as<const char*>());
    TEST_ASSERT_EQUAL_STRING("VERSION", doc["request"].as<const char*>());
    TEST_ASSERT_EQUAL_STRING("OK", doc["result"].as<const char*>());
    TEST_ASSERT_EQUAL_STRING("1.7", doc["api"].as<const char*>());
}

// === Тесты формата SET команды ===

void test_set_command_format_heads(void) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "heads=[%.1f,%d]", 30.0f, 180);
    TEST_ASSERT_EQUAL_STRING("heads=[30.0,180]", buf);
}

void test_set_command_format_hearts(void) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "hearts=[%.1f,%d]", 2.5f, 5);
    TEST_ASSERT_EQUAL_STRING("hearts=[2.5,5]", buf);
}

void test_set_command_format_hyst(void) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "hyst=%.2f", 0.25f);
    TEST_ASSERT_EQUAL_STRING("hyst=0.25", buf);
}

void setup() {
    delay(2000);
    UNITY_BEGIN();
    RUN_TEST(test_api_speed_limits);
    RUN_TEST(test_api_hyst_limits);
    RUN_TEST(test_api_decrement_limits);
    RUN_TEST(test_api_tank_mmhg_limits);
    RUN_TEST(test_api_heads_timer_max);
    RUN_TEST(test_isValidSpeed_valid);
    RUN_TEST(test_isValidSpeed_timeExceedsPeriod);
    RUN_TEST(test_isValidSpeed_timeOverMax);
    RUN_TEST(test_isValidSpeed_periodOverMax);
    RUN_TEST(test_clampSpeed_enforcesLimits);
    RUN_TEST(test_clampSpeed_timeNotExceedPeriod);
    RUN_TEST(test_isValidHyst);
    RUN_TEST(test_isValidDecrement);
    RUN_TEST(test_isValidTankMmhg);
    RUN_TEST(test_isValidHeadsTimer);
    RUN_TEST(test_parseSpeedParam);
    RUN_TEST(test_parseSpeedParam_heads_example);
    RUN_TEST(test_get_settings_response_structure);
    RUN_TEST(test_version_response_structure);
    RUN_TEST(test_set_command_format_heads);
    RUN_TEST(test_set_command_format_hearts);
    RUN_TEST(test_set_command_format_hyst);
    UNITY_END();
}

void loop() {}
