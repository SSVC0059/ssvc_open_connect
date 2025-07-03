#ifndef TELEGRAMBOTSUBSYSTEM_H
#define TELEGRAMBOTSUBSYSTEM_H

#include "core/subsystem/SubsystemManager.h"
#include <external/telegramm/TelegramBotClient.h>
#include <core/OpenConnectSettings/OpenConnectSettings.h> // при необходимости доступа к настройкам


class TelegramBotSubsystem final : public Subsystem {
public:
    static std::string getName() { return "telegram_bot"; }

    void initialize() override {
        if (!_initialized) {
            ESP_LOGI("TelegramBotSubsystem", "Initializing Telegram Bot subsystem");
            // Подготовка бота, токена, хендлеров и т.п.
            bot = &TelegramBotClient::bot();
            if (bot == nullptr || !bot->isReadiness())
            {
                _initialized = false;
                disable();
            }
            _initialized = true;
        }
    }

    void enable() override {
        if (!_initialized) {
            ESP_LOGD("TelegramBotSubsystem", "Subsystem not initialized!");
            return;
        }
        bot->init();
        if (!_enabled) {
            ESP_LOGI("TelegramBotSubsystem", "Enabling Telegram Bot subsystem");
            _enabled = true;
        }
    }

    void disable() override {
        if (!_enabled) return;

        ESP_LOGI("TelegramBotSubsystem", "Disabling...");
        bot->shutoff();
        _enabled = false;
        vTaskDelay(pdMS_TO_TICKS(100)); // Даём время на завершение операций
    }

    void setSettingsService(OpenConnectSettingsService& service) {
        _settingsService = &service;
    }

    void setHttpServer(PsychicHttpServer& server) {
        _server = &server;
    }
private:
    OpenConnectSettingsService* _settingsService = nullptr;
    PsychicHttpServer* _server = nullptr; // <-- сохраняем ссылк
    TelegramBotClient* bot = nullptr;
    bool _initialized = false;
    bool _enabled = false;
};

#endif // TELEGRAMBOTSUBSYSTEM_H
