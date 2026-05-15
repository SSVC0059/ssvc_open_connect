//
// Created by Admin on 23.01.2026.
#include "TelegramBotSubsystem.h"
#include "external/telegramm/TelegramBotClient.h"
#include "core/SubsystemManager/SubsystemManager.h"

namespace {

void onTelegramBootDone(const bool ok, void* ctx) {
    auto* self = static_cast<TelegramBotSubsystem*>(ctx);
    if (!self) return;
    if (ok) {
        self->markEnabled();
        ESP_LOGI("TelegramBotSubsystem", "Telegram Bot ready");
    } else {
        ESP_LOGE("TelegramBotSubsystem", "Boot failed — disabling subsystem in NVS");
        SubsystemManager::instance().disableSubsystem(TelegramBotSubsystem::getName());
    }
}

}  // namespace

void TelegramBotSubsystem::initialize() {
    if (!_initialized) {
        ESP_LOGI("TelegramBotSubsystem", "Initializing Telegram Bot subsystem");
        bot = &TelegramBotClient::bot();
        // isReadiness проверяется после init() — здесь только регистрируем указатель.
        _initialized = (bot != nullptr);
        if (!_initialized) {
            ESP_LOGE("TelegramBotSubsystem", "TelegramBotClient singleton unavailable");
        }
    }
}

void TelegramBotSubsystem::enable() {
    if (!_initialized) {
        ESP_LOGD("TelegramBotSubsystem", "Subsystem not initialized!");
        return;
    }

    if (_enabled && bot->isApiReady()) {
        return;
    }

    TelegramSettingsService* settingsService = TelegramSettingsService::getInstance();
    if (!settingsService) {
        ESP_LOGE("TelegramBotSubsystem", "Failed to get TelegramSettingsService instance!");
        SubsystemManager::instance().disableSubsystem(getName());
        return;
    }

    if (bot->isBootInProgress()) {
        ESP_LOGD("TelegramBotSubsystem", "Boot already in progress");
        return;
    }

    if (bot->isApiReady()) {
        _enabled = true;
        return;
    }

    ESP_LOGI("TelegramBotSubsystem", "Starting Telegram Bot (background boot)");
    if (!bot->startBoot(settingsService, onTelegramBootDone, this)) {
        SubsystemManager::instance().disableSubsystem(getName());
    }
}

void TelegramBotSubsystem::disable() {
    ESP_LOGI("TelegramBotSubsystem", "Disabling...");
    if (bot) {
        bot->shutoff();
    }
    _enabled = false;
    vTaskDelay(pdMS_TO_TICKS(100));
}