//
// Created by demoncat on 05.05.25.
//

#ifndef TELEGRAMBOT_H
#define TELEGRAMBOT_H

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
