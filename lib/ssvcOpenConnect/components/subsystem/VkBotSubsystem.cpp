#include "VkBotSubsystem.h"

#include <Features.h>

#if FT_ENABLED(FT_VK_BOT)

#include "external/vk/VkMessengerClient.h"
#include "core/StatefulServices/VkSettingsService/VkSettingsService.h"

void VkBotSubsystem::initialize() {
    if (!_initialized) {
        ESP_LOGI("VkBotSubsystem", "init");
        _client = &VkMessengerClient::instance();
        if (_client == nullptr) {
            _initialized = false;
            disable();
            return;
        }
        _initialized = true;
    }
}

void VkBotSubsystem::enable() {
    if (!_initialized) {
        ESP_LOGD("VkBotSubsystem", "not initialized");
        return;
    }
    VkSettingsService* svc = VkSettingsService::getInstance();
    if (!svc) {
        ESP_LOGE("VkBotSubsystem", "VkSettingsService null");
        return;
    }
    _client->init(svc);
    _enabled = true;
    ESP_LOGI("VkBotSubsystem", "enabled");
}

void VkBotSubsystem::disable() {
    if (!_enabled) {
        return;
    }
    ESP_LOGI("VkBotSubsystem", "disable");
    if (_client) {
        _client->shutoff();
    }
    _enabled = false;
    vTaskDelay(pdMS_TO_TICKS(50));
}

#else

void VkBotSubsystem::initialize() {}
void VkBotSubsystem::enable() {}
void VkBotSubsystem::disable() {}

#endif
