#include "OpenConnectHandler.h"

/**
*   SSVC Open Connect
 *
 *   A firmware for ESP32 to interface with SSVC 0059 distillation controller
 *   via UART protocol. Features a responsive SvelteKit web interface for
 *   monitoring and controlling the distillation process.
 *   https://github.com/SSVC0059/ssvc_open_connect
 *
 *   Copyright (C) 2024 SSVC Open Connect Contributors
 *
 *   This software is independent and not affiliated with SSVC0059 company.
 *   All Rights Reserved. This software may be modified and distributed under
 *   the terms of the LGPL v3 license. See the LICENSE file for details.
 *
 *   Disclaimer: Use at your own risk. High voltage safety precautions required.
 **/

#include "core/SsvcOpenConnect.h"
#include "core/SsvcSettings/SsvcSettings.h"
#include "core/StatefulServices/OpenConnectHardwareSettingsService/OpenConnectHardwareConfig.h"
#include "core/StatefulServices/OpenConnectHardwareSettingsService/OpenConnectHardwareSettingsService.h"
#include "core/RelayRuleEngine/RelayRuleEngine.h"
#include "core/RelayPortCoordinator/RelayPortCoordinator.h"
#include "core/AlarmMonitor/AlarmMonitor.h"
#include "core/SsvcConnector.h"
#include "core/rectification/RectificationParseUtils.h"
#include "components/sensors/SensorManager/SensorManager.h"
#include <vector>
#include <algorithm>
#include <cstring>
#include "components/subsystem/I2CBusSubsystem.h"
#include "components/subsystem/Xkc25Subsystem.h"

namespace {
std::vector<uint8_t> normalizedRelayAddresses(const std::vector<uint8_t>& configured);

const char* hardwareFaultCodeKey(const HardwareFaultCode code) {
    switch (code) {
        case HardwareFaultCode::NONE: return "none";
        case HardwareFaultCode::I2C_NACK: return "i2c_nack";
        case HardwareFaultCode::I2C_BUS_DOWN: return "i2c_bus_down";
        case HardwareFaultCode::DEVICE_NOT_PRESENT: return "device_not_present";
        case HardwareFaultCode::UART_LINK_LOST: return "uart_link_lost";
        default: return "unknown";
    }
}

const char* hardwareFaultCodeLabel(const HardwareFaultCode code) {
    switch (code) {
        case HardwareFaultCode::NONE: return "Нет сбоя";
        case HardwareFaultCode::I2C_NACK: return "I2C: нет подтверждения (NACK)";
        case HardwareFaultCode::I2C_BUS_DOWN: return "I2C: шина недоступна";
        case HardwareFaultCode::DEVICE_NOT_PRESENT: return "Устройство не обнаружено";
        case HardwareFaultCode::UART_LINK_LOST: return "Потеря связи с SSVC (UART)";
        default: return "Неизвестный код";
    }
}
}

OpenConnectHandler::OpenConnectHandler() = default;

/**
 * Активные аппаратные сбои.
 *
 * До этого список читали только сам модуль дисплея (чтобы показать первую ошибку на
 * экране) и RelayRuleEngine — в интерфейсе причина отказа оборудования не отображалась,
 * и приходилось смотреть serial-лог.
 */
void OpenConnectHandler::getHardwareFaults(AsyncWebServerRequest* request)
{
    AsyncJsonResponse* response = new AsyncJsonResponse();
    JsonObject root = response->getRoot();

    JsonArray faults = root["faults"].to<JsonArray>();
    for (const auto& fault : AlarmMonitor::getInstance().getActiveHardwareFaults()) {
        JsonObject item = faults.add<JsonObject>();
        item["code"] = hardwareFaultCodeKey(fault.code);
        item["codeLabel"] = hardwareFaultCodeLabel(fault.code);
        item["address"] = fault.i2c_addr;
        item["role"] = fault.role.c_str();
    }
    root["count"] = faults.size();
    // Шина отдельным полем: при выключенной подсистеме i2c_bus отказы устройств
    // не поднимаются, и пустой список сбоев читается иначе.
    I2CBusSubsystem& bus = I2CBusSubsystem::getInstance();
    root["i2cBusActive"] = bus.isActive();

    // Список ответивших адресов: по нему видно, выпало одно устройство или вся шина.
    JsonArray scan = root["i2cScan"].to<JsonArray>();
    for (const uint8_t address : bus.scanAddresses(false)) {
        scan.add(address);
    }

    response->setLength();
    request->send(response);
}

void OpenConnectHandler::getInfo(AsyncWebServerRequest* request)
{
    AsyncJsonResponse* response = new AsyncJsonResponse();
    JsonObject root = response->getRoot();

    const SsvcSettings& settings = SsvcSettings::init();

    JsonObject ssvc = root["ssvc"].to<JsonObject>();
    ssvc["version"] = settings.getSsvcVersion().c_str();
    ssvc["api"] = settings.getSsvcApiVersion().c_str();
    ssvc["api_code"] = settings.getSsvcApiVersionCode();
    ssvc["mode"] = SsvcSettings::init().isSupportTails() ? "tails" : "late_heads";

    JsonObject oc = root["oc"].to<JsonObject>();
    oc["version"] = APP_VERSION;
    oc["is_support_api"] = settings.apiSsvcIsSupport();
    // Признак ветки прошивки: «Сброс и снижение» есть только у прошивки
    // с подголовниками (late_heads), у прошивки с хвостами (tails) — нет.
    oc["is_support_release"] = settings.isSupportRelease();
    oc["api_min"] = SSVC_API_VERSION_MIN;
    oc["api_target"] = SSVC_API_VERSION_TARGET;
    oc["api_compatibility"] =
        SsvcApiCapabilities::compatibilityName(settings.getSsvcApiVersionCode());
    JsonArray apiFeatures = oc["api_features"].to<JsonArray>();
    for (int feature = 0; feature < SsvcApiCapabilities::FEATURE_COUNT; ++feature) {
        JsonObject item = apiFeatures.add<JsonObject>();
        item["name"] = SsvcApiCapabilities::featureName(feature);
        item["min_api"] = SsvcUartApiSpec::formatApiVersion(
                              SsvcApiCapabilities::featureMinVersion(feature))
                              .c_str();
        item["available"] = SsvcApiCapabilities::featureAvailable(
            settings.getSsvcApiVersionCode(), feature);
    }

    response->setLength();
    request->send(response);
}

void OpenConnectHandler::getRelayCapabilities(AsyncWebServerRequest* request) {
#if PINOUT_USE_GPIO
    request->send(200, "application/json", R"({"supported":false})");
#else
    AsyncJsonResponse* response = new AsyncJsonResponse();
    JsonObject root = response->getRoot();
    root["supported"] = true;
    JsonObject pcf = root["pcf8574"].to<JsonObject>();
    RelayRuleEngine::getInstance().appendCapabilitiesJson(pcf);
    response->setLength();
    request->send(response);
#endif
}

void OpenConnectHandler::getRelayMetadata(AsyncWebServerRequest* request) {
#if PINOUT_USE_GPIO
    request->send(200, "application/json", R"({"supported":false})");
#else
    AsyncJsonResponse* response = new AsyncJsonResponse();
    JsonObject root = response->getRoot();
    root["supported"] = true;

    std::vector<uint8_t> relayAddrs;
    OpenConnectHardwareSettingsService::instance().read([&](OpenConnectHardwareConfig& cfg) {
        relayAddrs = cfg.relayPcf8574Addresses;
    });
    relayAddrs = normalizedRelayAddresses(relayAddrs);

    const unsigned chips = static_cast<unsigned>(relayAddrs.size());
    const unsigned lineCount = chips * SSVC_RELAY_PCF8574_LINES_PER_CHIP;
    root["relayChipCount"] = chips;
    root["relayLineCount"] = lineCount;

    JsonArray relays = root["relays"].to<JsonArray>();
    unsigned displayIndex = 1;
    for (unsigned chip = 0; chip < chips; ++chip) {
        for (unsigned local = 0; local < SSVC_RELAY_PCF8574_LINES_PER_CHIP; ++local) {
            const unsigned global = chip * SSVC_RELAY_PCF8574_LINES_PER_CHIP + local;
            JsonObject relay = relays.add<JsonObject>();
            relay["index"] = displayIndex++;
            relay["label"] = String("Реле ") + String(global + 1);
            const bool reserved =
                (chip == 0 &&
                 (local == static_cast<unsigned>(SSVC_RELAY_PCF8574_BIT_DANGEROUS) ||
                  local == static_cast<unsigned>(SSVC_RELAY_PCF8574_BIT_CRITICAL)));
            relay["editable"] = !reserved;
            if (reserved) {
                relay["reason"] = "Системное реле AlarmManager (только просмотр)";
            }
            relay["targetBit"] = global;
        }
    }

    JsonArray sensorNames = root["sensorNames"].to<JsonArray>();
    const auto& sensors = SensorManager::getInstance().getAllSensors();
    for (const auto& pair : sensors) {
        if (!pair.second) {
            continue;
        }
        const std::string& name = pair.second->getName();
        if (!name.empty()) {
            sensorNames.add(name.c_str());
        }
    }

    JsonArray hardwareFaultCodes = root["hardwareFaultCodes"].to<JsonArray>();
    const HardwareFaultCode codes[] = {
        HardwareFaultCode::NONE,
        HardwareFaultCode::I2C_NACK,
        HardwareFaultCode::I2C_BUS_DOWN,
        HardwareFaultCode::DEVICE_NOT_PRESENT,
        HardwareFaultCode::UART_LINK_LOST,
    };
    for (const auto code : codes) {
        JsonObject item = hardwareFaultCodes.add<JsonObject>();
        item["value"] = static_cast<int>(code);
        item["key"] = hardwareFaultCodeKey(code);
        item["label"] = hardwareFaultCodeLabel(code);
    }

    JsonArray hardwareRoles = root["hardwareRoles"].to<JsonArray>();
    hardwareRoles.add("i2c_bus");
    hardwareRoles.add("ssvc_uart");
    static const char* kExtraPcfRoles[] = {
        SSVC_RELAY_PCF8574_DEVICE_ROLE, "pcf8574_port1", "pcf8574_port2", "pcf8574_port3",
        "pcf8574_port4", "pcf8574_port5", "pcf8574_port6", "pcf8574_port7"};
    for (unsigned c = 0; c < chips && c < 8; ++c) {
        hardwareRoles.add(kExtraPcfRoles[c]);
    }
    for (const auto& role : AlarmMonitor::getInstance().getKnownHardwareRoles()) {
        hardwareRoles.add(role.c_str());
    }

    JsonArray rectificationStages = root["rectificationStages"].to<JsonArray>();
    const char* stages[] = {
        "waiting", "tp1_waiting", "delayed_start", "heads",
        "late_heads", "hearts", "tails", "settings"
    };
    for (const char* stage : stages) {
        JsonObject st = rectificationStages.add<JsonObject>();
        st["value"] = stage;
        st["label"] = RectificationParseUtils::translateStage(stage).c_str();
    }

    JsonArray ssvcFields = root["ssvcFields"].to<JsonArray>();
    RelayRuleEngine::getInstance().appendSsvcFieldsJson(ssvcFields);

    response->setLength();
    request->send(response);
#endif
}

void OpenConnectHandler::getRelayState(AsyncWebServerRequest* request) {
#if PINOUT_USE_GPIO
    request->send(200, "application/json", R"({"supported":false})");
#else
    AsyncJsonResponse* response = new AsyncJsonResponse();
    JsonObject root = response->getRoot();
    root["supported"] = true;
    auto& coord = RelayPortCoordinator::getInstance();
    root["userShadow"] = coord.effectiveShadowChip(0);
    JsonArray shadows = root["userShadows"].to<JsonArray>();
    for (size_t i = 0; i < coord.relayChipCount(); ++i) {
        shadows.add(coord.effectiveShadowChip(static_cast<unsigned>(i)));
    }
    response->setLength();
    request->send(response);
#endif
}

void OpenConnectHandler::postRelayOverride(AsyncWebServerRequest* request, JsonVariant& json) {
#if PINOUT_USE_GPIO
    request->send(400, "application/json", R"({"error":"PCF8574 relay path not enabled in this build"})");
    return;
#else
    if (!json.is<JsonObject>()) {
        request->send(400, "application/json", R"({"error":"Invalid JSON"})");
        return;
    }
    JsonObject o = json.as<JsonObject>();
    if (!o["bit"].is<int>()) {
        request->send(400, "application/json", R"({"error":"Missing bit"})");
        return;
    }
    const int bit = o["bit"].as<int>();
    if (bit < 0) {
        request->send(400, "application/json", R"({"error":"Invalid bit"})");
        return;
    }
    auto& coord = RelayPortCoordinator::getInstance();
    const unsigned total = coord.totalRelayLines();
    if (static_cast<unsigned>(bit) >= total) {
        request->send(400, "application/json", R"({"error":"bit out of range"})");
        return;
    }
    if (coord.isAlarmReservedGlobal(static_cast<unsigned>(bit))) {
        request->send(403, "application/json", R"({"error":"Relay is reserved for AlarmManager"})");
        return;
    }
    const bool enable = o["enable"] | true;
    const bool energized = o["energized"] | false;
    RelayRuleEngine::getInstance().setManualOverride(static_cast<unsigned>(bit), energized, enable);
    request->send(200, "application/json", R"({"ok":true})");
#endif
}

namespace {

std::vector<uint8_t> normalizedRelayAddresses(const std::vector<uint8_t>& configured) {
    std::vector<uint8_t> relayAddrs = configured;
    if (relayAddrs.empty()) {
        relayAddrs.push_back(SSVC_RELAY_PCF8574_I2C_ADDR);
    }
    if (relayAddrs.size() > OpenConnectHardwareConfig::kMaxRelayChips) {
        relayAddrs.resize(OpenConnectHardwareConfig::kMaxRelayChips);
    }
    return relayAddrs;
}

void appendHardwareConfigCapabilities(JsonObject root) {
    JsonObject cap = root["capabilities"].to<JsonObject>();
#if PINOUT_USE_GPIO
    cap["pinoutUseGpio"] = true;
    cap["relayOptionsAvailable"] = false;
#else
    cap["pinoutUseGpio"] = false;
    cap["relayOptionsAvailable"] = true;
#endif
}

#if !PINOUT_USE_GPIO
void appendRelayProbeStatus(JsonObject root, TwoWire* w, const std::vector<uint8_t>& configured) {
    const std::vector<uint8_t> relayAddrs = normalizedRelayAddresses(configured);
    JsonArray probeOk = root["relayPcf8574ProbeOk"].to<JsonArray>();
    bool relayHwOk = true;
    for (uint8_t addr : relayAddrs) {
        bool ok = false;
        if (w) {
            w->beginTransmission(addr);
            ok = (w->endTransmission() == 0);
        }
        probeOk.add(ok);
        if (!ok) {
            relayHwOk = false;
        }
    }
    root["relayHardwareOk"] = relayHwOk;
}
#endif

} // namespace

void OpenConnectHandler::getHardwareConfig(AsyncWebServerRequest* request) {
    auto& svc = OpenConnectHardwareSettingsService::instance();
    AsyncJsonResponse* response = new AsyncJsonResponse();
    JsonObject root = response->getRoot();

    bool needs = false;
    svc.read([&](OpenConnectHardwareConfig& s) {
        OpenConnectHardwareConfig::read(s, root);
        needs = s.pendingReboot;
        // Все проверки идут одной пачкой под блокировкой шины, иначе рабочий
        // поток дисплея/RTC вклинится между beginTransmission() и endTransmission().
        I2CBusSubsystem::Guard bus(I2CBusSubsystem::getInstance());
        TwoWire* w = I2CBusSubsystem::getInstance().getWire();
        if (s.pressureSensorEnabled) {
            bool bmpOk = false;
            if (w) {
                w->beginTransmission(s.bmp581I2cAddress);
                bmpOk = (w->endTransmission() == 0);
            }
            root["bmp581ProbeOk"] = bmpOk;
        }
        if (s.rtcEnabled) {
            bool rtcOk = false;
            if (w) {
                w->beginTransmission(s.ds3231I2cAddress);
                rtcOk = (w->endTransmission() == 0);
            }
            root["ds3231ProbeOk"] = rtcOk;
        }
        if (s.lcd1602Enabled) {
            bool lcdOk = false;
            if (w) {
                w->beginTransmission(s.lcd1602I2cAddress);
                lcdOk = (w->endTransmission() == 0);
            }
            root["lcd1602ProbeOk"] = lcdOk;
        }
#if !PINOUT_USE_GPIO
        appendRelayProbeStatus(root, w, s.relayPcf8574Addresses);
#endif
    });
    appendHardwareConfigCapabilities(root);
    root["needsReboot"] = needs;

    response->setLength();
    request->send(response);
}

void OpenConnectHandler::putHardwareConfig(AsyncWebServerRequest* request, JsonVariant& json) {
    if (!json.is<JsonObject>()) {
        request->send(400);
        return;
    }

    JsonObject body = json.as<JsonObject>();
    const StateUpdateResult result = OpenConnectHardwareSettingsService::instance().update(
        body,
        OpenConnectHardwareConfig::update,
        "rest");

    if (result == StateUpdateResult::ERROR) {
        request->send(400);
        return;
    }

    AsyncJsonResponse* response = new AsyncJsonResponse();
    JsonObject root = response->getRoot();

    bool needs = false;
    OpenConnectHardwareSettingsService::instance().read([&](OpenConnectHardwareConfig& s) {
        OpenConnectHardwareConfig::read(s, root);
        needs = s.pendingReboot;
        I2CBusSubsystem::Guard bus(I2CBusSubsystem::getInstance());
        TwoWire* w = I2CBusSubsystem::getInstance().getWire();
        if (s.pressureSensorEnabled) {
            bool bmpOk = false;
            if (w) {
                w->beginTransmission(s.bmp581I2cAddress);
                bmpOk = (w->endTransmission() == 0);
            }
            root["bmp581ProbeOk"] = bmpOk;
        }
        if (s.rtcEnabled) {
            bool rtcOk = false;
            if (w) {
                w->beginTransmission(s.ds3231I2cAddress);
                rtcOk = (w->endTransmission() == 0);
            }
            root["ds3231ProbeOk"] = rtcOk;
        }
        if (s.lcd1602Enabled) {
            bool lcdOk = false;
            if (w) {
                w->beginTransmission(s.lcd1602I2cAddress);
                lcdOk = (w->endTransmission() == 0);
            }
            root["lcd1602ProbeOk"] = lcdOk;
        }
#if !PINOUT_USE_GPIO
        appendRelayProbeStatus(root, w, s.relayPcf8574Addresses);
#endif
    });
    appendHardwareConfigCapabilities(root);
    root["needsReboot"] = needs;

    response->setLength();
    request->send(response);
}

namespace {

/** Предел ожидания на шаге «проверка возврата»: шаг блокирующий, дольше держать HTTP-запрос нельзя. */
constexpr uint32_t kMaxVerifyReturnTimeoutMs = 5000;

/**
 * Статус одного слота XKC-25: конфигурация из файла плюс живое состояние пина.
 *
 * rawLevel — это текущее чтение пина, а не подтверждение наличия датчика: у GPIO нет
 * подтверждения устройства, поэтому вместо пробинга отдаётся пара rawLevel + calibrated.
 * live = false и rawLevel = -1 означают, что датчик выключен или подсистема его не подняла.
 */
void appendXkc25SensorStatus(JsonObject o, size_t slot, const Xkc25SensorConfig& cfg,
                             uint8_t sharedTankIndex, Xkc25Sensor* live) {
    // У переносного датчика фактическая зона — та ёмкость, на которой он стоит сейчас.
    const uint8_t zoneIndex = cfg.sharedAcrossTanks ? sharedTankIndex : cfg.zoneIndex;
    DigitalInputSource src;
    src.source = cfg.pinSource;
    src.gpio = cfg.gpio;
    src.pcfAddress = cfg.pcfAddress;
    src.pcfBit = cfg.pcfBit;
    const std::string pinLabel = src.label();

    o["index"] = static_cast<unsigned>(slot);
    o["enabled"] = cfg.enabled;
    o["pinSource"] = cfg.pinSource;
    o["gpio"] = cfg.gpio;
    o["pcfAddress"] = cfg.pcfAddress;
    o["pcfBit"] = cfg.pcfBit;
    o["pinLabel"] = pinLabel.c_str();
    o["zoneIndex"] = zoneIndex;
    o["zone"] = Xkc25Subsystem::zoneKey(zoneIndex).c_str();
    o["zoneLabel"] = Xkc25Subsystem::zoneLabel(zoneIndex).c_str();
    o["sharedAcrossTanks"] = cfg.sharedAcrossTanks;
    o["activeLow"] = cfg.activeLow;
    o["calibrated"] = cfg.calibrated;
    o["calibratedAt"] = cfg.calibratedAt;
    o["levelWithoutLiquid"] = cfg.levelWithoutLiquid;
    o["levelWithLiquid"] = cfg.levelWithLiquid;
    o["live"] = (live != nullptr);
    o["rawLevel"] = (live != nullptr) ? live->readRawPinLevel() : -1;
    o["filled"] = (live != nullptr) && live->isFilled();
}

/** HTTP-статус ответа на шаг калибровки по машинному коду итога. */
int calibrationHttpStatus(const char* code) {
    if (code == nullptr) {
        return 500;
    }
    if (strcmp(code, Xkc25CalibrationCode::OK) == 0 ||
        strcmp(code, Xkc25CalibrationCode::VERIFY_FAILED) == 0) {
        // Обратная проверка не блокирующая: шаг выполнен, результат — предупреждение в теле.
        return 200;
    }
    if (strcmp(code, Xkc25CalibrationCode::SENSOR_NOT_AVAILABLE) == 0 ||
        strcmp(code, Xkc25CalibrationCode::SENSOR_NOT_CONFIGURED) == 0) {
        return 404;
    }
    if (strcmp(code, Xkc25CalibrationCode::PERSIST_FAILED) == 0) {
        return 500;
    }
    // unstable_signal / no_external_pull / not_reacted / not_calibrated —
    // состояние мешает шагу, файл не изменён.
    return 409;
}

/** JSON шага калибровки: итог плюс актуальное состояние датчика после шага. */
String buildXkc25CalibrationBody(size_t slot, const Xkc25CalibrationResult& result) {
    JsonDocument doc;
    doc["ok"] = result.ok;
    doc["completed"] = result.completed;
    doc["code"] = result.code;
    doc["rawLevel"] = result.rawLevel;
    doc["message"] = result.message;

    JsonArray checklist = doc["checklist"].to<JsonArray>();
    for (const auto& item : result.checklist) {
        checklist.add(item);
    }

    JsonObject sensor = doc["sensor"].to<JsonObject>();
    Xkc25Subsystem* subsystem = Xkc25Subsystem::activeInstance();
    OpenConnectHardwareSettingsService::instance().read([&](OpenConnectHardwareConfig& cfg) {
        if (slot < cfg.xkc25Sensors.size()) {
            Xkc25Sensor* live = (subsystem != nullptr)
                                    ? subsystem->sensorByIndex(static_cast<uint8_t>(slot))
                                    : nullptr;
            appendXkc25SensorStatus(sensor, slot, cfg.xkc25Sensors[slot],
                                    cfg.xkc25SharedTankIndex, live);
        }
    });

    String body;
    serializeJson(doc, body);
    return body;
}

} // namespace

void OpenConnectHandler::getXkc25Status(AsyncWebServerRequest* request) {
    AsyncJsonResponse* response = new AsyncJsonResponse();
    JsonObject root = response->getRoot();

    Xkc25Subsystem* subsystem = Xkc25Subsystem::activeInstance();

    JsonArray sensors = root["sensors"].to<JsonArray>();
    OpenConnectHardwareSettingsService::instance().read([&](OpenConnectHardwareConfig& cfg) {
        for (size_t slot = 0; slot < cfg.xkc25Sensors.size(); ++slot) {
            Xkc25Sensor* live = (subsystem != nullptr)
                                    ? subsystem->sensorByIndex(static_cast<uint8_t>(slot))
                                    : nullptr;
            appendXkc25SensorStatus(sensors.add<JsonObject>(), slot, cfg.xkc25Sensors[slot],
                                    cfg.xkc25SharedTankIndex, live);
        }
    });

    // Общие опции: UI показывает выбранный режим реакции и текущую ёмкость переносного датчика.
    uint8_t triggerAction = OpenConnectHardwareConfig::kXkc25ActionPause;
    uint8_t sharedTankIndex = 0;
    OpenConnectHardwareSettingsService::instance().read([&](OpenConnectHardwareConfig& cfg) {
        triggerAction = cfg.xkc25TriggerAction;
        sharedTankIndex = cfg.xkc25SharedTankIndex;
    });
    root["triggerAction"] = triggerAction;
    root["sharedTankIndex"] = sharedTankIndex;

    // Предстартовая проверка: UI показывает, будет ли старт отклонён в режиме block (6.4).
    const Xkc25StartCheck check = Xkc25Subsystem::checkStart();
    root["startGuardMode"] = check.mode;
    root["blockStart"] = !check.allow;
    JsonArray blockReasons = root["blockReasons"].to<JsonArray>();
    for (const auto& reason : check.blockReasons) {
        blockReasons.add(reason);
    }
    JsonArray warnings = root["warnings"].to<JsonArray>();
    for (const auto& warning : check.warnings) {
        warnings.add(warning);
    }

    response->setLength();
    request->send(response);
}

void OpenConnectHandler::postXkc25Calibrate(AsyncWebServerRequest* request, JsonVariant& json) {
    if (!json.is<JsonObject>()) {
        request->send(400, "application/json", R"({"error":"Invalid JSON"})");
        return;
    }
    JsonObject body = json.as<JsonObject>();

    if (!body["index"].is<int>()) {
        request->send(400, "application/json", R"({"error":"Missing index"})");
        return;
    }
    const int index = body["index"].as<int>();
    if (index < 0 || static_cast<size_t>(index) >= OpenConnectHardwareConfig::kMaxXkc25Sensors) {
        request->send(400, "application/json", R"({"error":"index out of range"})");
        return;
    }

    const char* step = body["step"] | "";
    if (step[0] == '\0') {
        request->send(400, "application/json", R"({"error":"Missing step"})");
        return;
    }

    Xkc25Subsystem* subsystem = Xkc25Subsystem::activeInstance();
    if (subsystem == nullptr) {
        // Датчики создаются при включении подсистемы: без перезагрузки калибровать нечего.
        request->send(404, "application/json",
                      R"({"error":"XKC-25 subsystem is not active","code":"sensor_not_available"})");
        return;
    }

    const auto slot = static_cast<uint8_t>(index);
    Xkc25CalibrationResult result;
    if (strcmp(step, "without_liquid") == 0) {
        result = subsystem->recordLevelWithoutLiquid(slot);
    } else if (strcmp(step, "with_liquid") == 0) {
        result = subsystem->recordLevelWithLiquid(slot);
    } else if (strcmp(step, "verify_return") == 0) {
        const uint32_t timeoutMs =
            std::min<uint32_t>(body["timeoutMs"] | 5000u, kMaxVerifyReturnTimeoutMs);
        result = subsystem->verifyReturn(slot, timeoutMs);
    } else {
        request->send(400, "application/json", R"({"error":"Unknown step"})");
        return;
    }

    request->send(calibrationHttpStatus(result.code), "application/json",
                  buildXkc25CalibrationBody(slot, result));
}
