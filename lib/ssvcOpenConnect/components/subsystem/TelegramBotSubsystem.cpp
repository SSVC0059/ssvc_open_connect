//
// Created by Admin on 23.01.2026.
//#include "TelegramBotSubsystem.h"
#include "external/telegramm/TelegramBotClient.h" // Теперь подключаем здесь

void TelegramBotSubsystem::initialize() {
    if (!_initialized) {
        ESP_LOGI("TelegramBotSubsystem", "Initializing Telegram Bot subsystem");
        bot = &TelegramBotClient::bot();
        if (bot == nullptr || !TelegramBotClient::isReadiness()) {
            _initialized = false;
            disable();
            return;
        }
        _initialized = true;
    }
}

void TelegramBotSubsystem::enable() {
    if (!_initialized) {
        ESP_LOGD("TelegramBotSubsystem", "Subsystem not initialized!");
        return;
    }

    TelegramSettingsService* settingsService = TelegramSettingsService::getInstance();
    if (!settingsService) {
        ESP_LOGE("TelegramBotSubsystem", "Failed to get TelegramSettingsService instance!");
        return;
    }

    bot->init(settingsService);
    if (!_enabled) {
        ESP_LOGI("TelegramBotSubsystem", "Enabling Telegram Bot subsystem");
        _enabled = true;
    }
}

void TelegramBotSubsystem::disable() {
    if (!_enabled) return;

    ESP_LOGI("TelegramBotSubsystem", "Disabling...");
    if (bot) bot->shutoff();
    _enabled = false;
    vTaskDelay(pdMS_TO_TICKS(100));
}