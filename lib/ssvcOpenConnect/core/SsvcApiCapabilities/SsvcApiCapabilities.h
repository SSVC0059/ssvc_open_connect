#ifndef SSVC_OPEN_CONNECT_SSVC_API_CAPABILITIES_H
#define SSVC_OPEN_CONNECT_SSVC_API_CAPABILITIES_H

/**
 *   SSVC Open Connect
 *
 *   Слой возможностей UART API: единая точка, которая решает, доступна ли
 *   конкретная функция openConnect на подключённом устройстве.
 *
 *   Версия API устройства приходит строкой ("1.7") и хранится целым кодом
 *   major * 100 + minor. Сравнивать версии как дробные числа нельзя: "1.10"
 *   в таком виде равно 1.1 и оказывается меньше "1.7".
 *
 *   Устройство старее прошивки не отключается целиком: функции, появившиеся
 *   позже его версии, отбрасываются по одной (деградация), а базовый набор
 *   продолжает работать.
 **/

#include <string>
#include <vector>

#include "core/SsvcUartApiSpec/SsvcUartApiSpec.h"

// Целевая версия API, которую реализует прошивка, и минимальная версия
// устройства, с которой она сохраняет работоспособность. Обе — строки вида
// "1.9", потому что версия обязана разбираться без дробной арифметики.
#ifndef SSVC_API_VERSION_TARGET
#define SSVC_API_VERSION_TARGET "1.9"
#endif

#ifndef SSVC_API_VERSION_MIN
#define SSVC_API_VERSION_MIN "1.7"
#endif

namespace SsvcApiCapabilities {

using SsvcUartApiSpec::ApiVersion;
using SsvcUartApiSpec::API_VERSION_UNKNOWN;

/** Возможности openConnect, зависящие от версии UART API устройства. */
enum class SsvcFeature : int {
    Base = 0,          // набор 1.7: базовые команды и параметры
    FormulaModes,      // formula=0/1
    Predec,            // predec=0/7/13
    ExtendedHeads,     // extended_heads=0/1
    TankMmhgAct,       // tank_mmhg_act в телеметрии
    FormulaAuto92Plus, // formula=2 (авто 92+)
    HeartsPreemptCnt,  // preempt_cnt в телеметрии тела
    Count
};

constexpr int FEATURE_COUNT = static_cast<int>(SsvcFeature::Count);

/**
 * Описание возможности: имя для логов и REST плюс минимальная версия API.
 *
 * Одна таблица вместо двух параллельных массивов: имя и версия возможности
 * физически не могут разойтись при добавлении новой записи.
 */
struct FeatureInfo {
    const char* name;
    ApiVersion minVersion;
};

// Версии выставлены по diff README репозитория спецификации
// (SmartModule-ru/ssvc0059_v2_uart_api_docs). Проверено: formula=[0|1],
// tank_mmhg_act, formula_start_temp, release_* и heads_final присутствуют уже
// в README 1.7, поэтому они базовые; в 1.8 добавлен только extended_heads
// (плюс поля телеметрии preempt_cnt / late_heads_finished), в 1.9 — predec и
// значение formula=2.
constexpr FeatureInfo FEATURE_INFO[FEATURE_COUNT] = {
    {"base", {1, 7}},               // набор 1.7: базовые команды и параметры
    {"formula", {1, 7}},            // formula=0/1
    {"predec", {1, 9}},             // predec=0/7/13
    {"extended_heads", {1, 8}},     // extended_heads=0/1
    {"tank_mmhg_act", {1, 7}},      // tank_mmhg_act в телеметрии
    {"formula_auto92", {1, 9}},     // formula=2 (авто 92+)
    {"hearts_preempt_cnt", {1, 8}}  // preempt_cnt, late_heads_finished в телах
};

// Именованные коды возможностей: позволяют проверять возможность из модулей,
// заголовки которых не тянут зависимость на этот файл.
constexpr int FEATURE_BASE = static_cast<int>(SsvcFeature::Base);
constexpr int FEATURE_FORMULA = static_cast<int>(SsvcFeature::FormulaModes);
constexpr int FEATURE_PREDEC = static_cast<int>(SsvcFeature::Predec);
constexpr int FEATURE_EXTENDED_HEADS = static_cast<int>(SsvcFeature::ExtendedHeads);
constexpr int FEATURE_TANK_MMHG_ACT = static_cast<int>(SsvcFeature::TankMmhgAct);
constexpr int FEATURE_FORMULA_AUTO92 = static_cast<int>(SsvcFeature::FormulaAuto92Plus);
constexpr int FEATURE_HEARTS_PREEMPT_CNT = static_cast<int>(SsvcFeature::HeartsPreemptCnt);

inline ApiVersion parseOr(const char* text, const ApiVersion& fallback) {
    ApiVersion parsed;
    return SsvcUartApiSpec::parseApiVersion(text, parsed) ? parsed : fallback;
}

/** Минимальная версия устройства, с которой работает прошивка. */
inline ApiVersion minSupportedApiVersion() {
    static const ApiVersion version = parseOr(SSVC_API_VERSION_MIN, ApiVersion{1, 7});
    return version;
}

/** Версия API, которую реализует прошивка. */
inline ApiVersion targetApiVersion() {
    static const ApiVersion version = parseOr(SSVC_API_VERSION_TARGET, ApiVersion{1, 9});
    return version;
}

inline int minSupportedApiVersionCode() { return minSupportedApiVersion().code(); }

inline int targetApiVersionCode() { return targetApiVersion().code(); }

/** Код 0 означает «версия устройства ещё неизвестна». */
inline bool isVersionKnown(const int deviceCode) { return deviceCode > 0; }

/** Совместимо ли устройство с прошивкой в принципе (не ниже минимальной версии). */
inline bool isCompatible(const int deviceCode) {
    return isVersionKnown(deviceCode) && deviceCode >= minSupportedApiVersionCode();
}

/** Состояние совместимости для REST/UI: unknown / too_old / supported. */
inline const char* compatibilityName(const int deviceCode) {
    if (!isVersionKnown(deviceCode)) {
        return "unknown";
    }
    return isCompatible(deviceCode) ? "supported" : "too_old";
}

/** Минимальная версия для возможности; неизвестная возможность — заведомо недоступна. */
inline ApiVersion featureMinVersion(const int feature) {
    if (feature < 0 || feature >= FEATURE_COUNT) {
        return ApiVersion{255, 99};
    }
    return FEATURE_INFO[feature].minVersion;
}

inline const char* featureName(const int feature) {
    if (feature < 0 || feature >= FEATURE_COUNT) {
        return "unknown";
    }
    return FEATURE_INFO[feature].name;
}

/**
 * Доступна ли возможность на устройстве.
 * Пока версия неизвестна, доступны только возможности минимальной версии:
 * команды, требующие более новых полей, не отправляются вслепую.
 */
inline bool featureAvailable(const int deviceCode, const int feature) {
    const int required = featureMinVersion(feature).code();
    if (!isVersionKnown(deviceCode)) {
        return required <= minSupportedApiVersionCode();
    }
    return deviceCode >= required;
}

/** Маркер параметра SET, которого нет в таблице версий. */
constexpr int FEATURE_UNKNOWN = -1;

/**
 * Параметры SET, существующие с версии 1.7.
 *
 * Список соответствует полному набору, который формирует
 * SsvcSettings::Builder (lib/ssvcOpenConnect/core/SsvcSettings/SsvcSettings.cpp).
 * Без явного перечня базовых полей нельзя решить, что делать с незнакомым:
 * отбросить его на старом устройстве или пропустить.
 */
constexpr const char* BASE_SET_PARAMETERS[] = {
    "heads", "hearts", "late_heads", "tails", "hyst", "decrement", "tank_mmhg",
    "tank_mmhg_act", "heads_timer", "late_heads_timer", "hearts_timer", "tails_temp",
    "start_delay", "hearts_finish_temp", "formula_start_temp", "valve_bw",
    "release_speed", "release_timer", "heads_final", "parallel", "parallel_v1",
    "parallel_v3", "s_temp", "s_hyst", "s_speed", "s_decrement", "s_timer"
};

constexpr int BASE_SET_PARAMETER_COUNT =
    sizeof(BASE_SET_PARAMETERS) / sizeof(BASE_SET_PARAMETERS[0]);

inline bool isBaseSetParam(const std::string& name) {
    for (int index = 0; index < BASE_SET_PARAMETER_COUNT; ++index) {
        if (name == BASE_SET_PARAMETERS[index]) {
            return true;
        }
    }
    return false;
}

/** Убирает пробелы и табы по краям: " formula " -> "formula". */
inline std::string trimCopy(const std::string& text) {
    const std::size_t begin = text.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        return std::string();
    }
    const std::size_t end = text.find_last_not_of(" \t\r\n");
    return text.substr(begin, end - begin + 1);
}

/** Снимает обрамляющие скобки у значения: "[2]" -> "2". */
inline std::string unwrapValue(const std::string& value) {
    if (value.size() >= 2 && value.front() == '[' && value.back() == ']') {
        return value.substr(1, value.size() - 2);
    }
    return value;
}

/** Требование параметра SET: возможность и минимальная версия устройства. */
struct SetParamRequirement {
    int feature;
    ApiVersion minVersion;

    // C++11: структура со значениями по умолчанию не является агрегатом,
    // поэтому конструкторы заданы явно — иначе SetParamRequirement{...} не компилируется.
    constexpr SetParamRequirement() : feature(FEATURE_BASE), minVersion(1, 7) {}

    constexpr SetParamRequirement(const int featureCode, const ApiVersion& version)
        : feature(featureCode), minVersion(version) {}
};

/**
 * Сопоставляет параметр SET ("predec=7") минимальной версии API.
 *
 * Параметры, появившиеся после 1.7, перечислены явно по матрице
 * docs/develop/api-version-status.md. Незнакомый параметр требует целевой
 * версии: поле, которого нет в таблице, не отправляется на устройство старее
 * прошивки. Базовые параметры перечислены в BASE_SET_PARAMETERS, поэтому
 * полный набор 1.7 уходит на старое устройство без изменений.
 */
inline SetParamRequirement requiredForSetParam(const std::string& parameter) {
    const std::size_t equals = parameter.find('=');
    if (equals == std::string::npos) {
        // Одиночное слово — не пара параметр=значение: считаем базовым.
        return SetParamRequirement{FEATURE_BASE, ApiVersion{1, 7}};
    }

    const std::string name = trimCopy(parameter.substr(0, equals));
    const std::string value = unwrapValue(trimCopy(parameter.substr(equals + 1)));

    if (name == "formula") {
        // formula=0/1 существует с 1.7, значение 2 («авто 92+») появилось в 1.9.
        return value == "2" ? SetParamRequirement{FEATURE_FORMULA_AUTO92, ApiVersion{1, 9}}
                            : SetParamRequirement{FEATURE_FORMULA, ApiVersion{1, 7}};
    }
    if (name == "predec") {
        return SetParamRequirement{FEATURE_PREDEC, ApiVersion{1, 9}};
    }
    if (name == "extended_heads") {
        return SetParamRequirement{FEATURE_EXTENDED_HEADS, ApiVersion{1, 8}};
    }
    if (isBaseSetParam(name)) {
        return SetParamRequirement{FEATURE_BASE, ApiVersion{1, 7}};
    }

    return SetParamRequirement{FEATURE_UNKNOWN, targetApiVersion()};
}

/** Возможность, требуемая для параметра команды SET (учитывает значение параметра). */
inline int requiredFeatureForSetParam(const std::string& parameter) {
    return requiredForSetParam(parameter).feature;
}

/**
 * Доступен ли параметр SET на устройстве.
 *
 * @param parameter параметр вида "name=value"
 * @param deviceCode код версии устройства (0 — версия ещё неизвестна)
 */
inline bool isSetParamAvailable(const std::string& parameter, const int deviceCode) {
    const int required = requiredForSetParam(parameter).minVersion.code();
    if (!isVersionKnown(deviceCode)) {
        // Пессимистично: пока версия не пришла, уходит только базовый набор.
        return required <= minSupportedApiVersionCode();
    }
    return deviceCode >= required;
}

/** Разбивает строку параметров SET по запятым, не трогая запятые внутри [..]. */
inline std::vector<std::string> splitSetParameters(const std::string& parameters) {
    std::vector<std::string> parts;
    std::string current;
    int depth = 0;
    for (const char symbol : parameters) {
        if (symbol == '[') {
            ++depth;
        } else if (symbol == ']' && depth > 0) {
            --depth;
        }
        if (symbol == ',' && depth == 0) {
            parts.push_back(current);
            current.clear();
            continue;
        }
        current += symbol;
    }
    parts.push_back(current);
    return parts;
}

/**
 * Убирает из строки параметров SET поля, недоступные на версии устройства.
 * @param skipped заполняется отброшенными параметрами.
 * @return true, если осталась хотя бы одна пара параметр=значение.
 */
inline bool filterSetParameters(const std::string& parameters, const int deviceCode,
                                std::vector<std::string>& skipped, std::string& filtered) {
    skipped.clear();
    filtered.clear();
    for (const std::string& part : splitSetParameters(parameters)) {
        if (part.empty()) {
            continue;
        }
        if (!isSetParamAvailable(part, deviceCode)) {
            skipped.push_back(part);
            continue;
        }
        if (!filtered.empty()) {
            filtered += ",";
        }
        filtered += part;
    }
    return !filtered.empty();
}

} // namespace SsvcApiCapabilities

#endif // SSVC_OPEN_CONNECT_SSVC_API_CAPABILITIES_H
