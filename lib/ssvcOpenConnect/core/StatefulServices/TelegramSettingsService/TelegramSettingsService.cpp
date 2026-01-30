#include "TelegramSettingsService.h"
#include <HttpEndpoint.h>

TelegramSettingsService* TelegramSettingsService::_instance = nullptr;

StateUpdateResult updateTelegramSettings(JsonObject& root, TelegramSettings& settings) {
    bool changed = false;
    if (root["botToken"].is<const char*>()) {
        if (settings.botToken != root["botToken"].as<String>()) {
            settings.botToken = root["botToken"].as<String>();
            changed = true;
        }
    }
    if (root["chatId"].is<const char*>()) {
        if (settings.chatId != root["chatId"].as<String>()) {
            settings.chatId = root["chatId"].as<String>();
            changed = true;
        }
    }
    return changed ? StateUpdateResult::CHANGED : StateUpdateResult::UNCHANGED;
}

void readTelegramSettings(TelegramSettings& settings, JsonObject& root) {
    root["botToken"] = settings.botToken;
    root["chatId"] = settings.chatId;
}

TelegramSettingsService* TelegramSettingsService::getInstance() {
    return _instance;
}

void TelegramSettingsService::setInstance(TelegramSettingsService* instance) {
    _instance = instance;
}

TelegramSettingsService::TelegramSettingsService(PsychicHttpServer* server, ESP32SvelteKit* esp32sveltekit) :
    StatefulService<TelegramSettings>(),
    _fsPersistence(readTelegramSettings, updateTelegramSettings, this, esp32sveltekit->getFS(), "/config/telegram.json"),
    _esp32sveltekit(esp32sveltekit)
{
    HttpEndpoint<TelegramSettings>* httpEndpoint = new HttpEndpoint<TelegramSettings>(
        readTelegramSettings,
        updateTelegramSettings,
        this,
        server,
        "/api/telegram/config",
        esp32sveltekit->getSecurityManager()
    );
    httpEndpoint->begin();
}

void TelegramSettingsService::begin() {
    if (!_esp32sveltekit->getFS()->exists("/config/telegram.json")) {
        migrateFromNvs();
    } else {
        _fsPersistence.readFromFS();
    }
}

void TelegramSettingsService::migrateFromNvs() {
    ESP_LOGI("TelegramSettings", "Attempting to migrate settings from NVS to file system...");

    const char* ns = "telegram_bot";
    String oldToken = GlobalConfig::config().get<String>(ns, "token", "");
    int64_t oldChatIdInt = GlobalConfig::config().get<int64_t>(ns, "chat_id", 0);
    String oldChatId = (oldChatIdInt == 0) ? "" : String(oldChatIdInt);

    if (!oldToken.isEmpty() || !oldChatId.isEmpty()) {
        ESP_LOGI("TelegramSettings", "Found old settings in NVS. Migrating...");

        StateUpdateResult result = update([&](TelegramSettings& settings) {
            settings.botToken = oldToken;
            settings.chatId = oldChatId;
            return StateUpdateResult::CHANGED;
        }, "migration");

        if (result == StateUpdateResult::CHANGED) {
            if (_esp32sveltekit->getFS()->exists("/config/telegram.json")) {
                GlobalConfig::config().clearNamespace(ns);
                ESP_LOGI("TelegramSettings", "Migration complete. Old NVS settings cleared.");
            } else {
                ESP_LOGE("TelegramSettings", "Migration failed: Could not write new config file!");
            }
        } else {
             ESP_LOGE("TelegramSettings", "Migration failed: State was not updated.");
        }

    } else {
        ESP_LOGI("TelegramSettings", "No old settings found in NVS. Creating default config file.");
        _fsPersistence.readFromFS();
    }
}

// IProfileObserver implementation
const char* TelegramSettingsService::getProfileKey() const {
    return "telegram";
}

void TelegramSettingsService::onProfileApply(const JsonObject& profile) {
    // Create a mutable copy to use with the existing update function
    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();
    obj.set(profile);

    update([&](TelegramSettings& settings) {
        return updateTelegramSettings(obj, settings);
    }, "profile_apply");
}

void TelegramSettingsService::onProfileSave(JsonObject& profile) {
    // Use the existing read function to populate the profile object
    read([&](const TelegramSettings& settings) {
        readTelegramSettings(const_cast<TelegramSettings&>(settings), profile);
    });
}
