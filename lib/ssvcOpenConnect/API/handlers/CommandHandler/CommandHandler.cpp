#include "CommandHandler.h"

#include "components/subsystem/Xkc25Subsystem.h"

#include <cstdlib>
#include <cstring>
#include <limits>

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

CommandHandler::CommandHandler() = default;

void CommandHandler::handleCommand(AsyncWebServerRequest* request, JsonVariant& json)
{
    if (!json.is<JsonObject>()) {
        request->send(400, "text/plain", "Invalid JSON format");
        return;
    }

    if (!json["commands"].is<std::string>()) {
        request->send(400, "text/plain", "Missing or invalid 'commands' field");
        return;
    }

    const std::string commandName = json["commands"].as<std::string>();
    ESP_LOGD("CommandHandler", "Received command: %s", commandName.c_str());

    const auto it = SsvcCommandsQueue::COMMAND_MAP.find(commandName);
    if (it == SsvcCommandsQueue::COMMAND_MAP.end()) {
        ESP_LOGW("CommandHandler", "Unknown command: %s", commandName.c_str());
        request->send(501, "text/plain", "Command not implemented");
        return;
    }

    // Предстартовая проверка XKC-25: защищает только команды, проходящие через ESP32
    // (веб-UI, MQTT, боты). Старт физической кнопкой SSVC-0059 так не перехватить.
    Xkc25StartCheck xkc25Check;
    if (commandName == "start") {
        xkc25Check = Xkc25Subsystem::checkStart();
        if (!xkc25Check.allow) {
            JsonDocument doc;
            doc["status"] = "rejected";
            doc["command"] = commandName;
            JsonArray reasons = doc["blockReasons"].to<JsonArray>();
            for (const auto& reason : xkc25Check.blockReasons) {
                reasons.add(reason);
            }
            String body;
            serializeJson(doc, body);
            ESP_LOGW("CommandHandler", "Command '%s' rejected by the XKC-25 start guard",
                     commandName.c_str());
            request->send(409, "application/json", body);
            return;
        }
    }

    try {
        std::string params;
        if (json["parameters"].is<std::string>()) {
            params = json["parameters"].as<std::string>();
        }

        // Команда сообщает, поставлена ли она в очередь: 200 отдаётся только при
        // фактическом принятии, иначе клиент не отличит отказ от выполнения.
        // commandId назначается внутри постановки и принадлежит именно этой
        // команде (не «последней принятой» — иначе гонка с MQTT/ботами).
        uint32_t commandId = 0;
        const bool accepted = it->second(params, &commandId);
        if (!accepted) {
            const std::string reason = SsvcCommandsQueue::getQueue().lastRejectionReason();
            ESP_LOGW("CommandHandler", "Command rejected: %s (reason: %s)",
                     commandName.c_str(), reason.c_str());

            if (reason == "feature_unsupported") {
                request->send(409, "text/plain", "Command unsupported by device API");
            } else if (reason == "queue_full") {
                request->send(503, "text/plain", "Command queue is full");
            } else {
                request->send(503, "text/plain", "Command not accepted");
            }
            return;
        }

        ESP_LOGI("CommandHandler", "Command accepted: %s (id=%u)", commandName.c_str(),
                 static_cast<unsigned>(commandId));

        // 200 — всегда один JSON-контракт: «принято в очередь» + commandId для
        // GET /rest/commands/{id}. Неблокирующие предупреждения XKC-25
        // добавляются отдельным полем, чтобы UI показал их до старта процесса.
        JsonDocument doc;
        doc["status"] = "accepted";
        doc["command"] = commandName;
        doc["commandId"] = commandId;
        if (!xkc25Check.warnings.empty()) {
            JsonArray warnings = doc["warnings"].to<JsonArray>();
            for (const auto& warning : xkc25Check.warnings) {
                warnings.add(warning);
            }
        }
        String body;
        serializeJson(doc, body);
        request->send(200, "application/json", body);
    } catch (const std::exception& e) {
        ESP_LOGE("CommandHandler", "Command error: %s", e.what());
        request->send(500, "text/plain", "Command execution failed");
    }
}

/**
 * @brief GET /rest/commands/{id}: судьба команды по commandId.
 *
 * 200 — запись журнала попыток (состояние, времена, ok/reason);
 * 400 — id не десятичное число или пустой; 404 — записи нет или истёк TTL.
 * Ответ всегда содержит confidence:"transport": это уверенность в транспорте
 * (запись ушла в UART и ответ пришёл/не пришёл), а не в том, что SSVC применил
 * команду.
 */
void CommandHandler::getCommand(AsyncWebServerRequest* request)
{
    constexpr const char* kPrefix = "/rest/commands/";
    const String rawId = request->url().substring(static_cast<unsigned int>(std::strlen(kPrefix)));

    // Строгий разбор: пусто, знак, пробелы и любые нецифровые символы — 400.
    // Иначе "abc" молча стал бы id 0 и отвечал бы 404 «нет записи».
    if (rawId.isEmpty() || rawId[0] < '0' || rawId[0] > '9') {
        request->send(400, "application/json", R"({"error":"invalid_id"})");
        return;
    }

    char* end = nullptr;
    const unsigned long long parsed = std::strtoull(rawId.c_str(), &end, 10);
    if (end == rawId.c_str() || *end != '\0' ||
        parsed > std::numeric_limits<uint32_t>::max()) {
        request->send(400, "application/json", R"({"error":"invalid_id"})");
        return;
    }

    SsvcCommandStatus status;
    if (!SsvcCommandsQueue::getQueue().getCommandStatus(static_cast<uint32_t>(parsed), status)) {
        // Нет записи или она вытеснена кольцом / истекла по TTL: не отдаём
        // чужую команду под этим id.
        request->send(404, "application/json", R"({"error":"not_found"})");
        return;
    }

    JsonDocument doc;
    doc["id"] = status.id;
    doc["command"] = status.command != nullptr ? status.command : "unknown";
    doc["state"] = SsvcCommandsQueue::commandStateName(status.state);
    doc["queued_at"] = status.queuedAt;
    if (status.startedAt != 0) {
        doc["started_at"] = status.startedAt;
    } else {
        doc["started_at"] = nullptr;
    }
    if (status.finishedAt != 0) {
        doc["finished_at"] = status.finishedAt;
    } else {
        doc["finished_at"] = nullptr;
    }
    if (status.ok < 0) {
        doc["ok"] = nullptr;
    } else {
        doc["ok"] = status.ok != 0;
    }
    if (status.reason != nullptr) {
        doc["reason"] = status.reason;
    } else {
        doc["reason"] = nullptr;
    }
    doc["confidence"] = "transport";

    String body;
    serializeJson(doc, body);
    request->send(200, "application/json", body);
}
