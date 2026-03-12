/**
 * Native unit tests for simple utility logic (runs on host: pio test -e native)
 */
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>
#include <string>
#include <cctype>

static bool isNumericId(const std::string& id) {
    if (id.empty()) return false;
    for (char c : id) {
        if (!std::isdigit(static_cast<unsigned char>(c))) return false;
    }
    return true;
}

TEST_CASE("Utils - IsNumericId Valid") {
    CHECK(isNumericId("0"));
    CHECK(isNumericId("1"));
    CHECK(isNumericId("123"));
    CHECK(isNumericId("999"));
}

TEST_CASE("Utils - IsNumericId Invalid") {
    CHECK_FALSE(isNumericId(""));
    CHECK_FALSE(isNumericId("a"));
    CHECK_FALSE(isNumericId("1a"));
    CHECK_FALSE(isNumericId("profile1"));
}
