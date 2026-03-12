/**
 * Native тесты SsvcUartApiSpec (логика без Arduino/ESP32)
 * SSVC0059_V2 UART API v1.7
 *
 * Документация:
 * - docs/develop/api/ssvc-uart-api.md — описание API, команды, формат SET
 * - https://github.com/SmartModule-ru/ssvc0059_v2_uart_api_docs/blob/v1.7/README.md — спецификация v1.7
 */
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>
#include <cstdio>
#include "core/SsvcUartApiSpec/SsvcUartApiSpec.h"

TEST_CASE("SsvcUartApiSpec - SpeedLimits") {
    CHECK(SsvcUartApiSpec::SPEED_TIME_MAX == doctest::Approx(99.9f));
    CHECK(SsvcUartApiSpec::SPEED_PERIOD_MAX == 999);
}

TEST_CASE("SsvcUartApiSpec - IsValidSpeed Valid") {
    CHECK(SsvcUartApiSpec::isValidSpeed(30.0f, 180));
    CHECK(SsvcUartApiSpec::isValidSpeed(99.0f, 100));
    CHECK(SsvcUartApiSpec::isValidSpeed(0.0f, 0));
}

TEST_CASE("SsvcUartApiSpec - IsValidSpeed TimeExceedsPeriod") {
    CHECK_FALSE(SsvcUartApiSpec::isValidSpeed(100.0f, 99));
}

TEST_CASE("SsvcUartApiSpec - ClampSpeed EnforcesLimits") {
    float t = 150.0f;
    int p = 1500;
    SsvcUartApiSpec::clampSpeed(t, p);
    CHECK(t == doctest::Approx(99.9f));
    CHECK(p == 999);
}

TEST_CASE("SsvcUartApiSpec - ClampSpeed TimeNotExceedPeriod") {
    float t = 80.0f;
    int p = 50;
    SsvcUartApiSpec::clampSpeed(t, p);
    CHECK(t == doctest::Approx(50.0f));
    CHECK(p == 50);
}

TEST_CASE("SsvcUartApiSpec - IsValidHyst") {
    CHECK(SsvcUartApiSpec::isValidHyst(0.25f));
    CHECK(SsvcUartApiSpec::isValidHyst(50.0f));
    CHECK_FALSE(SsvcUartApiSpec::isValidHyst(50.01f));
}

TEST_CASE("SsvcUartApiSpec - IsValidDecrement") {
    CHECK(SsvcUartApiSpec::isValidDecrement(20));
    CHECK(SsvcUartApiSpec::isValidDecrement(100));
    CHECK_FALSE(SsvcUartApiSpec::isValidDecrement(101));
}

TEST_CASE("SsvcUartApiSpec - ParseSpeedParam") {
    float t;
    int p;
    CHECK(SsvcUartApiSpec::parseSpeedParam("[30.0,180]", t, p));
    CHECK(t == doctest::Approx(30.0f));
    CHECK(p == 180);
}

TEST_CASE("SsvcUartApiSpec - SetCommandFormat") {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "heads=[%.1f,%d]", 30.0f, 180);
    CHECK(std::string(buf) == "heads=[30.0,180]");
}
