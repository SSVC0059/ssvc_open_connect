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
    static esp_err_t setToken(PsychicRequest* request);
    static esp_err_t setChatId(PsychicRequest* request);
    static esp_err_t getToken(PsychicRequest* request);
    static esp_err_t getChatId(PsychicRequest* request);

private:
    static bool checkSubsystemEnabled();
    static constexpr const char* TAG = "TelegramBot";
};

#endif //TELEGRAMBOT_H
