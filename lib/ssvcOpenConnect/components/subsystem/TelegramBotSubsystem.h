#ifndef TELEGRAMBOTSUBSYSTEM_H
#define TELEGRAMBOTSUBSYSTEM_H

#include "core/SubsystemManager/SubsystemManager.h"
#include "core/StatefulServices/TelegramSettingsService/TelegramSettingsService.h"

// Опережающее объявление вместо #include "TelegramBotClient.h"
class TelegramBotClient;

class TelegramBotSubsystem final : public Subsystem {
public:
    static std::string getName() { return "telegram_bot"; }

    void initialize() override;
    void enable() override;
    void disable() override;

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