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

#include <ESPAsyncWebServer.h>
#include <AsyncJson.h>

class TelegramBotHandler {
public:
    TelegramBotHandler();

    static void settings(AsyncWebServerRequest* request);
    static void updateSettings(AsyncWebServerRequest* request, JsonVariant& json);
    static void getSettings(AsyncWebServerRequest* request);

private:
    static bool checkSubsystemEnabled();
    static void setToken(AsyncWebServerRequest* request);
    static void setChatId(AsyncWebServerRequest* request);
    static constexpr const char* TAG = "TelegramBot";
};

#endif //TELEGRAMBOT_H
