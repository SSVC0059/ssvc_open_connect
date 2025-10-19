#ifndef TELEGRAMBOTSUBSYSTEM_H
#define TELEGRAMBOTSUBSYSTEM_H

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

#include "core/SubsystemManager/SubsystemManager.h"
#include "external/telegramm/TelegramBotClient.h"

class TelegramBotSubsystem final : public Subsystem {
public:
    static std::string getName() { return "telegram_bot"; }

    void initialize() override {
        if (!_initialized) {
            ESP_LOGI("TelegramBotSubsystem", "Initializing Telegram Bot subsystem");
            // Подготовка бота, токена, хендлеров и т.п.
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
