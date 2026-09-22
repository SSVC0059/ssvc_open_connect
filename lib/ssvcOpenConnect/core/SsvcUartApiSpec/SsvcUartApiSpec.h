/**
 * SSVC0059_V2 UART API Specification (v1.9)
 * https://github.com/SmartModule-ru/ssvc0059_v2_uart_api_docs/blob/v1.9/README.md
 *
 * Константы и валидация по спецификации API.
 * Используется для тестов и проверки соответствия форматов.
 */
#ifndef SSVC_UART_API_SPEC_H
#define SSVC_UART_API_SPEC_H

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>

namespace SsvcUartApiSpec {

// === Ограничения API (из документации) ===

// heads, hearts, late_heads, tails, parallel, parallel_v1, s_speed
constexpr float SPEED_TIME_MAX = 99.9f;
constexpr int SPEED_PERIOD_MAX = 999;

// hyst
constexpr float HYST_MIN = 0.0f;
constexpr float HYST_MAX = 50.0f;

// decrement
constexpr int DECREMENT_MIN = 0;
constexpr int DECREMENT_MAX = 100;

// tank_mmhg
constexpr float TANK_MMHG_MIN = 0.0f;
constexpr float TANK_MMHG_MAX = 50.0f;

// heads_timer, late_heads_timer (кратно 300)
constexpr unsigned int HEADS_TIMER_MAX = 86400;
constexpr unsigned int HEADS_TIMER_STEP = 300;

// hearts_timer
constexpr int HEARTS_TIMER_MAX = 30;

// start_delay
constexpr unsigned int START_DELAY_MAX = 18000;

// hearts_finish_temp, tails_temp, formula_start_temp
constexpr float TEMP_MAX = 110.0f;
constexpr float FORMULA_START_TEMP_MIN = 84.0f;
constexpr float FORMULA_START_TEMP_MAX = 100.0f;

// valve_bw
constexpr int VALVE_BW_MAX = 20000;

// release_timer
constexpr int RELEASE_TIMER_MAX = 1200;

// Максимальная длина строки SET (включая \n и \0)
constexpr size_t SET_CMD_MAX_LEN = 300;

// Команды UART API
#define SSVC_CMD_AT "AT"
#define SSVC_CMD_NEXT "NEXT"
#define SSVC_CMD_PAUSE "PAUSE"
#define SSVC_CMD_RESUME "RESUME"
#define SSVC_CMD_STOP "STOP"
#define SSVC_CMD_START "START"
#define SSVC_CMD_STATUS "STATUS"
#define SSVC_CMD_VERSION "VERSION"
#define SSVC_CMD_GET_SETTINGS "GET_SETTINGS"
#define SSVC_CMD_SET "SET"

// === Валидация (соответствие API) ===

/** Проверка: время_включения ≤ 99.9, период ≤ 999, время_включения ≤ период */
inline bool isValidSpeed(float timeTurnOn, int period) {
    return timeTurnOn >= 0.0f && timeTurnOn <= SPEED_TIME_MAX &&
           period >= 0 && period <= SPEED_PERIOD_MAX &&
           timeTurnOn <= static_cast<float>(period);
}

/** Ограничение значения speed по API */
inline void clampSpeed(float& timeTurnOn, int& period) {
    timeTurnOn = std::min(std::max(timeTurnOn, 0.0f), SPEED_TIME_MAX);
    period = std::min(std::max(period, 0), SPEED_PERIOD_MAX);
    if (timeTurnOn > static_cast<float>(period)) {
        timeTurnOn = static_cast<float>(period);
    }
}

/** Проверка hyst: 0..50.0 */
inline bool isValidHyst(float v) {
    return v >= HYST_MIN && v <= HYST_MAX;
}

/** Проверка decrement: 0..100 */
inline bool isValidDecrement(int v) {
    return v >= DECREMENT_MIN && v <= DECREMENT_MAX;
}

/** Проверка tank_mmhg: 0..50 */
inline bool isValidTankMmhg(float v) {
    return v >= TANK_MMHG_MIN && v <= TANK_MMHG_MAX;
}

/** Проверка heads_timer: 0..86400, кратно 300 */
inline bool isValidHeadsTimer(unsigned int v) {
    return v <= HEADS_TIMER_MAX && (v % HEADS_TIMER_STEP) == 0;
}

/** Проверка formula: 0, 1 или 2 */
inline bool isValidFormula(int v) {
    return v == 0 || v == 1 || v == 2;
}

/** Проверка предекремента: 0, 7 или 13 */
inline bool isValidPredec(int v) {
    return v == 0 || v == 7 || v == 13;
}

/** Проверка режима extended_heads: 0 или 1 */
inline bool isValidExtendedHeads(int v) {
    return v == 0 || v == 1;
}

/** Парсинг формата [time,period] — возвращает true если строка соответствует */
inline bool parseSpeedParam(const char* param, float& outTime, int& outPeriod) {
    if (!param) return false;
    return std::sscanf(param, "[%f,%d]", &outTime, &outPeriod) == 2;
}

// === Версионирование API ===

// Версия API кодируется целым числом major * 100 + minor.
// Дробное представление недопустимо: "1.10" как float равно 1.1, то есть меньше
// "1.7", из-за чего любое сравнение версий ломается на двузначном minor.
constexpr int API_VERSION_MINOR_LIMIT = 99;

struct ApiVersion {
    uint8_t major;
    uint8_t minor;

    // C++11: структура со значениями по умолчанию агрегатом не является,
    // поэтому конструкторы заданы явно — иначе ApiVersion{1, 7} не компилируется.
    constexpr ApiVersion() : major(0), minor(0) {}

    constexpr ApiVersion(const uint8_t majorValue, const uint8_t minorValue)
        : major(majorValue), minor(minorValue) {}

    constexpr int code() const {
        return static_cast<int>(major) * 100 + static_cast<int>(minor);
    }

    constexpr bool isSet() const { return major != 0 || minor != 0; }
};

constexpr ApiVersion API_VERSION_UNKNOWN{0, 0};

constexpr bool operator==(const ApiVersion& lhs, const ApiVersion& rhs) {
    return lhs.code() == rhs.code();
}

constexpr bool operator!=(const ApiVersion& lhs, const ApiVersion& rhs) {
    return lhs.code() != rhs.code();
}

constexpr bool operator<(const ApiVersion& lhs, const ApiVersion& rhs) {
    return lhs.code() < rhs.code();
}

constexpr bool operator>(const ApiVersion& lhs, const ApiVersion& rhs) {
    return lhs.code() > rhs.code();
}

constexpr bool operator<=(const ApiVersion& lhs, const ApiVersion& rhs) {
    return lhs.code() <= rhs.code();
}

constexpr bool operator>=(const ApiVersion& lhs, const ApiVersion& rhs) {
    return lhs.code() >= rhs.code();
}

/**
 * Строгий разбор версии вида "1.10" или " 1.7 ".
 * Отвергает пустую строку, версию без minor ("1"), лишние символы ("1.7.2"),
 * нечисловой minor ("1.x") и minor больше 99.
 */
inline bool parseApiVersion(const char* text, ApiVersion& out) {
    if (text == nullptr) {
        return false;
    }

    const char* cursor = text;
    while (*cursor == ' ' || *cursor == '\t') {
        ++cursor;
    }

    int major = 0;
    int majorDigits = 0;
    while (*cursor >= '0' && *cursor <= '9') {
        major = major * 10 + (*cursor - '0');
        ++cursor;
        if (++majorDigits > 3) {
            return false;
        }
    }
    if (majorDigits == 0) {
        return false;
    }

    if (*cursor != '.' && *cursor != ',') {
        return false;
    }
    ++cursor;

    int minor = 0;
    int minorDigits = 0;
    while (*cursor >= '0' && *cursor <= '9') {
        minor = minor * 10 + (*cursor - '0');
        ++cursor;
        if (++minorDigits > 2) {
            return false;
        }
    }
    if (minorDigits == 0) {
        return false;
    }

    while (*cursor == ' ' || *cursor == '\t' || *cursor == '\r' || *cursor == '\n') {
        ++cursor;
    }
    if (*cursor != '\0') {
        return false;
    }

    if (major > 255 || minor > API_VERSION_MINOR_LIMIT) {
        return false;
    }

    out.major = static_cast<uint8_t>(major);
    out.minor = static_cast<uint8_t>(minor);
    return true;
}

inline bool parseApiVersion(const std::string& text, ApiVersion& out) {
    return parseApiVersion(text.c_str(), out);
}

/** Код версии из строки; 0 означает «версия неизвестна или не распознана». */
inline int apiVersionCode(const char* text) {
    ApiVersion version;
    return parseApiVersion(text, version) ? version.code() : 0;
}

inline int apiVersionCode(const std::string& text) {
    return apiVersionCode(text.c_str());
}

/** Форматирование версии для вывода: "1.10". */
inline std::string formatApiVersion(const ApiVersion& version) {
    return std::to_string(static_cast<unsigned>(version.major)) + "." +
           std::to_string(static_cast<unsigned>(version.minor));
}

/** Форматирование кода версии; для 0 возвращает пустую строку. */
inline std::string formatApiVersionCode(const int code) {
    if (code <= 0) {
        return "";
    }
    return formatApiVersion(ApiVersion{static_cast<uint8_t>(code / 100),
                                      static_cast<uint8_t>(code % 100)});
}

} // namespace SsvcUartApiSpec

#endif // SSVC_UART_API_SPEC_H
