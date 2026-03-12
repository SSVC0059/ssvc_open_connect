/**
 * SSVC0059_V2 UART API Specification (v1.7)
 * https://github.com/SmartModule-ru/ssvc0059_v2_uart_api_docs/blob/v1.7/README.md
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

/** Проверка formula: 0 или 1 */
inline bool isValidFormula(int v) {
    return v == 0 || v == 1;
}

/** Парсинг формата [time,period] — возвращает true если строка соответствует */
inline bool parseSpeedParam(const char* param, float& outTime, int& outPeriod) {
    if (!param) return false;
    return std::sscanf(param, "[%f,%d]", &outTime, &outPeriod) == 2;
}

} // namespace SsvcUartApiSpec

#endif // SSVC_UART_API_SPEC_H
