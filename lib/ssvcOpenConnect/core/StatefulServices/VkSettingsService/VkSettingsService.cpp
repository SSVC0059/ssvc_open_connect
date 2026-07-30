#include "VkSettingsService.h"
#include <HttpEndpoint.h>

VkSettingsService* VkSettingsService::_instance = nullptr;

static void applyDefaultVersion(VkSettings& s) {
    if (s.apiVersion.isEmpty()) {
        s.apiVersion = "5.199";
    }
}

StateUpdateResult updateVkSettings(JsonObject& root, VkSettings& settings) {
    bool changed = false;
    auto assignStr = [&](const char* key, String& field) {
        if (!root[key].is<const char*>() && !root[key].is<String>()) {
            return;
        }
        const String v = root[key].as<String>();
        if (field != v) {
            field = v;
            changed = true;
        }
    };

    assignStr("access_token", settings.accessToken);
    assignStr("api_version", settings.apiVersion);
    assignStr("group_id", settings.groupId);
    assignStr("peer_id", settings.peerId);
    if (settings.peerId.isEmpty()) {
        const char* legacyKeys[] = {"live_peer_id", "alerts_peer_id", "summary_peer_id"};
        for (const char* key : legacyKeys) {
            if (!root[key].is<const char*>() && !root[key].is<String>()) {
                continue;
            }
            const String v = root[key].as<String>();
            if (!v.isEmpty()) {
                settings.peerId = v;
                changed = true;
                break;
            }
        }
    }

    if (root["live_enabled"].is<bool>()) {
        const bool v = root["live_enabled"].as<bool>();
        if (settings.liveEnabled != v) {
            settings.liveEnabled = v;
            changed = true;
        }
    }
    if (root["alerts_enabled"].is<bool>()) {
        const bool v = root["alerts_enabled"].as<bool>();
        if (settings.alertsEnabled != v) {
            settings.alertsEnabled = v;
            changed = true;
        }
    }
    if (root["summary_enabled"].is<bool>()) {
        const bool v = root["summary_enabled"].as<bool>();
        if (settings.summaryEnabled != v) {
            settings.summaryEnabled = v;
            changed = true;
        }
    }
    if (root["wall_post_enabled"].is<bool>()) {
        const bool v = root["wall_post_enabled"].as<bool>();
        if (settings.wallPostEnabled != v) {
            settings.wallPostEnabled = v;
            changed = true;
        }
    }

    applyDefaultVersion(settings);
    return changed ? StateUpdateResult::CHANGED : StateUpdateResult::UNCHANGED;
}

void readVkSettings(VkSettings& settings, JsonObject& root) {
    applyDefaultVersion(settings);
    root["access_token"] = settings.accessToken;
    root["api_version"] = settings.apiVersion.isEmpty() ? "5.199" : settings.apiVersion;
    root["group_id"] = settings.groupId;
    root["peer_id"] = settings.peerId;
    root["live_enabled"] = settings.liveEnabled;
    root["alerts_enabled"] = settings.alertsEnabled;
    root["summary_enabled"] = settings.summaryEnabled;
    root["wall_post_enabled"] = settings.wallPostEnabled;
}

VkSettingsService* VkSettingsService::getInstance() {
    return _instance;
}

void VkSettingsService::setInstance(VkSettingsService* instance) {
    _instance = instance;
}

VkSettingsService::VkSettingsService(AsyncWebServer* server, ESP32SvelteKit* esp32sveltekit) :
    StatefulService<VkSettings>(),
    _fsPersistence(
        readVkSettings,
        [](JsonObject& root, VkSettings& s, const String&) { return updateVkSettings(root, s); },
        this,
        esp32sveltekit->getFS(),
        "/config/vk.json"),
    _esp32sveltekit(esp32sveltekit)
{
    auto* httpEndpoint = new HttpEndpoint<VkSettings>(
        readVkSettings,
        [](JsonObject& root, VkSettings& s, const String&) { return updateVkSettings(root, s); },
        this,
        server,
        "/api/vk/config",
        esp32sveltekit->getSecurityManager()
    );
    httpEndpoint->begin();
}

void VkSettingsService::begin() {
    _fsPersistence.readFromFS();
    update([&](VkSettings& s) {
        if (s.apiVersion.isEmpty()) {
            s.apiVersion = "5.199";
            return StateUpdateResult::CHANGED;
        }
        return StateUpdateResult::UNCHANGED;
    }, "begin");
}

const char* VkSettingsService::getProfileKey() const {
    return "vk";
}

void VkSettingsService::onProfileApply(const JsonObject& profile) {
    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();
    obj.set(profile);
    update([&](VkSettings& settings) { return updateVkSettings(obj, settings); }, "profile_apply");
}

void VkSettingsService::onProfileSave(JsonObject& profile) {
    read([&](const VkSettings& settings) {
        readVkSettings(const_cast<VkSettings&>(settings), profile);
    });
}
