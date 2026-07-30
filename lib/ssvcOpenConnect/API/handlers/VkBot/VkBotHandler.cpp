#include "VkBotHandler.h"

#include <ArduinoJson.h>
#include <Features.h>
#include <HTTPClient.h>
#include <WiFi.h>

#include "core/SubsystemManager/SubsystemManager.h"
#include "core/StatefulServices/VkSettingsService/VkSettingsService.h"

#if FT_ENABLED(FT_VK_BOT)

namespace {

void appendUrlEncoded(const char* plain, String& dst) {
    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(plain); *p; ++p) {
        const unsigned char c = *p;
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' ||
            c == '_' || c == '.' || c == '~') {
            dst += static_cast<char>(c);
        } else if (c == ' ') {
            dst += '+';
        } else {
            static const char* hex = "0123456789ABCDEF";
            dst += '%';
            dst += hex[(c >> 4) & 0xF];
            dst += hex[c & 0xF];
        }
    }
}

bool fetchVkConversations(const String& token, const String& apiVer, JsonArray peersOut, String& errMsg) {
    if (!WiFi.isConnected()) {
        errMsg = "WiFi not connected";
        return false;
    }
    if (token.isEmpty()) {
        errMsg = "access_token empty";
        return false;
    }

    String form = "access_token=";
    appendUrlEncoded(token.c_str(), form);
    form += "&v=";
    appendUrlEncoded(apiVer.c_str(), form);
    form += "&count=20&filter=all&extended=1";

    HTTPClient http;
    http.setTimeout(15000);
    if (!http.begin("https://api.vk.com/method/messages.getConversations")) {
        errMsg = "http.begin failed";
        return false;
    }
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    const int code = http.POST(form);
    const String payload = http.getString();
    http.end();

    if (code != HTTP_CODE_OK || payload.isEmpty()) {
        errMsg = "HTTP " + String(code);
        return false;
    }

    JsonDocument doc;
    if (deserializeJson(doc, payload) != DeserializationError::Ok) {
        errMsg = "JSON parse failed";
        return false;
    }
    if (doc["error"].is<JsonObject>()) {
        errMsg = String("VK ") + String(doc["error"]["error_code"].as<int>()) + ": " +
                 doc["error"]["error_msg"].as<const char*>();
        return false;
    }

    JsonArray items = doc["response"]["items"].as<JsonArray>();
    if (items.isNull()) {
        errMsg = "no items in response";
        return false;
    }

    for (JsonObject item : items) {
        JsonObject peer = item["conversation"]["peer"].as<JsonObject>();
        if (peer.isNull() || peer["id"].isNull()) {
            continue;
        }
        JsonObject row = peersOut.add<JsonObject>();
        row["peer_id"] = peer["id"].as<long long>();
        if (peer["type"].is<const char*>()) {
            row["type"] = peer["type"].as<const char*>();
        }
        if (item["chat_settings"]["title"].is<const char*>()) {
            row["title"] = item["chat_settings"]["title"].as<const char*>();
        } else if (item["last_message"]["from_id"].is<int64_t>()) {
            row["title"] = String("from_id ") + item["last_message"]["from_id"].as<int64_t>();
        }
    }
    return true;
}

}  // namespace

void VkBotHandler::getSettings(AsyncWebServerRequest* request) {
    AsyncJsonResponse* response = new AsyncJsonResponse();
    JsonObject root = response->getRoot();

    const std::string subsystemName = "vk_bot";
    const bool enable = SubsystemManager::instance().isSubsystemEnabled(subsystemName);
    root["enable"] = enable;
    root["subsystem_name"] = subsystemName.c_str();
    if (!enable) {
        root["status"] = "vk_bot disabled";
        response->setLength();
        String str;
        serializeJson(root, str);
        request->send(423, "application/json", str);
        delete response;
        return;
    }

    VkSettingsService* svc = VkSettingsService::getInstance();
    if (!svc) {
        request->send(500);
        delete response;
        return;
    }
    svc->read([&](const VkSettings& s) { readVkSettings(const_cast<VkSettings&>(s), root); });
    response->setLength();
    request->send(response);
}

void VkBotHandler::updateSettings(AsyncWebServerRequest* request, JsonVariant& json) {
    AsyncJsonResponse* response = new AsyncJsonResponse();
    JsonObject root = response->getRoot();
    bool result = false;

    if (!json.is<JsonObject>()) {
        request->send(400, "text/plain", "Invalid JSON format");
        delete response;
        return;
    }

    VkSettingsService* svc = VkSettingsService::getInstance();
    if (!svc) {
        request->send(500);
        delete response;
        return;
    }

    JsonObject body = json.as<JsonObject>();
    svc->update(
        [&](VkSettings& settings) { return updateVkSettings(body, settings); },
        "api");

    result = true;
    root["result"] = result;
    response->setLength();
    request->send(response);
}

void VkBotHandler::listConversationPeers(AsyncWebServerRequest* request, JsonVariant& json) {
    AsyncJsonResponse* response = new AsyncJsonResponse();
    JsonObject root = response->getRoot();

    String token;
    String apiVer = "5.199";

    VkSettingsService* svc = VkSettingsService::getInstance();
    if (svc) {
        svc->read([&](const VkSettings& s) {
            token = s.accessToken;
            apiVer = s.apiVersion.isEmpty() ? "5.199" : s.apiVersion;
        });
    }

    if (json.is<JsonObject>()) {
        const JsonObject body = json.as<JsonObject>();
        if (body["access_token"].is<const char*>()) {
            token = body["access_token"].as<String>();
        }
        if (body["api_version"].is<const char*>()) {
            apiVer = body["api_version"].as<String>();
        }
    }
    if (apiVer.isEmpty()) {
        apiVer = "5.199";
    }

    JsonArray peers = root["peers"].to<JsonArray>();
    String errMsg;
    if (!fetchVkConversations(token, apiVer, peers, errMsg)) {
        root["ok"] = false;
        root["error"] = errMsg;
        response->setLength();
        String out;
        serializeJson(root, out);
        request->send(502, "application/json", out);
        delete response;
        return;
    }

    root["ok"] = true;
    response->setLength();
    request->send(response);
}

#else

void VkBotHandler::getSettings(AsyncWebServerRequest* request) {
    request->send(404);
}

void VkBotHandler::updateSettings(AsyncWebServerRequest* request, JsonVariant&) {
    request->send(404);
}

void VkBotHandler::listConversationPeers(AsyncWebServerRequest* request, JsonVariant&) {
    request->send(404);
}

#endif
