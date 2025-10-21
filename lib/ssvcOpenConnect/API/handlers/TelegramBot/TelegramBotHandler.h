#ifndef TELEGRAMBOT_H
#define TELEGRAMBOT_H

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

#include "PsychicHttp.h"

class TelegramBotHandler {
public:
    TelegramBotHandler();

    static esp_err_t settings(PsychicRequest* request);
    static esp_err_t updateSettings(PsychicRequest* request);
    static esp_err_t getSettings(PsychicRequest* request);

private:
    static bool checkSubsystemEnabled();
    static esp_err_t setToken(PsychicRequest* request);
    static esp_err_t setChatId(PsychicRequest* request);
    static constexpr const char* TAG = "TelegramBot";
};

#endif //TELEGRAMBOT_H
