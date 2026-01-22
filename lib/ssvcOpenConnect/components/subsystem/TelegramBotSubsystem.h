#ifndef TELEGRAMBOTSUBSYSTEM_H
#define TELEGRAMBOTSUBSYSTEM_H

#include "core/SubsystemManager/SubsystemManager.h"
#include "external/telegramm/TelegramBotClient.h"
#include "core/StatefulServices/TelegramSettingsService/TelegramSettingsService.h"

class TelegramBotSubsystem final : public Subsystem {
public:
    static std::string getName() { return "telegram_bot"; }

    void initialize() override {
        if (!_initialized) {
            ESP_LOGI("TelegramBotSubsystem", "Initializing Telegram Bot subsystem");
            bot = &TelegramBotClient::bot();
            if (bot == nullptr || !TelegramBotClient::isReadiness())
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

    void disable() override {
        if (!_enabled) return;

        ESP_LOGI("TelegramBotSubsystem", "Disabling...");
        bot->shutoff();
        _enabled = false;
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    void setSettingsService(SsvcMqttSettingsService& service) {
        _ssvcMqttSettingsService = &service;
    }

    void setHttpServer(PsychicHttpServer& server) {
        _server = &server;
    }
private:
    SsvcMqttSettingsService* _ssvcMqttSettingsService = nullptr;
    PsychicHttpServer* _server = nullptr;
    TelegramBotClient* bot = nullptr;
    bool _initialized = false;
    bool _enabled = false;
};

#endif // TELEGRAMBOTSUBSYSTEM_H
