/**
 * Native unit tests for StringView (runs on host: pio test -e native)
 */
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>
#include <string>
#include "commons/StringView.h"

TEST_CASE("StringView - ConstructFromCharPtrAndSize") {
    const char* str = "hello";
    StringView sv(str, 5);
    CHECK(sv.data() == str);
    CHECK(sv.size() == 5u);
    CHECK_FALSE(sv.empty());
}

TEST_CASE("StringView - ConstructFromStdString") {
    std::string s = "world";
    StringView sv(s);
    CHECK(sv.data() == s.data());
    CHECK(sv.size() == s.size());
    CHECK(sv.size() == 5u);
}

TEST_CASE("StringView - EmptyString") {
    StringView sv("", 0);
    CHECK(sv.size() == 0u);
    CHECK(sv.empty());
}

TEST_CASE("StringView - SubstringView") {
    const char* str = "hello world";
    StringView sv(str + 6, 5);
    CHECK(sv.size() == 5u);
    CHECK(std::string(sv.data(), sv.size()) == "world");
}
