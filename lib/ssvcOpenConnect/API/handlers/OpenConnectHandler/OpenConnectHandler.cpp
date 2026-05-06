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
#include "components/subsystem/I2CBusSubsystem.h"

namespace {
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

void OpenConnectHandler::getInfo(AsyncWebServerRequest* request)
{
    AsyncJsonResponse* response = new AsyncJsonResponse();
    JsonObject root = response->getRoot();

    const SsvcSettings& settings = SsvcSettings::init();

    JsonObject ssvc = root["ssvc"].to<JsonObject>();
    ssvc["version"] = settings.getSsvcVersion().c_str();
    ssvc["api"] = settings.getSsvcApiVersion();
    ssvc["mode"] = SsvcSettings::init().isSupportTails() ? "tails" : "late_heads";

    JsonObject oc = root["oc"].to<JsonObject>();
    oc["version"] = APP_VERSION;
    oc["is_support_api"] = settings.apiSsvcIsSupport();

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
    if (relayAddrs.empty()) {
        relayAddrs.push_back(SSVC_RELAY_PCF8574_I2C_ADDR);
    }

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
    root["userShadow"] = coord.userShadow();
    JsonArray shadows = root["userShadows"].to<JsonArray>();
    for (size_t i = 0; i < coord.relayChipCount(); ++i) {
        shadows.add(coord.userShadowChip(static_cast<unsigned>(i)));
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
    const unsigned total = RelayPortCoordinator::getInstance().totalRelayLines();
    if (static_cast<unsigned>(bit) >= total) {
        request->send(400, "application/json", R"({"error":"bit out of range"})");
        return;
    }
    const bool enable = o["enable"] | true;
    const bool energized = o["energized"] | false;
    RelayRuleEngine::getInstance().setManualOverride(static_cast<unsigned>(bit), energized, enable);
    request->send(200, "application/json", R"({"ok":true})");
#endif
}

namespace {

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

} // namespace

void OpenConnectHandler::getHardwareConfig(AsyncWebServerRequest* request) {
    auto& svc = OpenConnectHardwareSettingsService::instance();
    AsyncJsonResponse* response = new AsyncJsonResponse();
    JsonObject root = response->getRoot();

    bool needs = false;
    svc.read([&](OpenConnectHardwareConfig& s) {
        OpenConnectHardwareConfig::read(s, root);
        needs = s.pendingReboot;
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
        JsonArray probeOk = root["relayPcf8574ProbeOk"].to<JsonArray>();
        bool relayHwOk = true;
        for (uint8_t addr : s.relayPcf8574Addresses) {
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
        JsonArray probeOk = root["relayPcf8574ProbeOk"].to<JsonArray>();
        bool relayHwOk = true;
        for (uint8_t addr : s.relayPcf8574Addresses) {
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
#endif
    });
    appendHardwareConfigCapabilities(root);
    root["needsReboot"] = needs;

    response->setLength();
    request->send(response);
}
