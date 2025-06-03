#ifndef TELEGRAM_BOT_CLIENT_H
#define TELEGRAM_BOT_CLIENT_H

#include <FastBot2.h>
#include <functional>
#include <unordered_map>
#include <WiFi.h>
#include "core/SsvcOpenConnect.h"
#include "core/OpenConnectSettings/GlobalConfig.h"
#include <subsystem/ThermalSubsystem.h>
#include <string_view>

#define FB_USE_LOG Serial   // выводить логи ошибок
#define MESSAGE_LIMIT_BY_UPDATE 3

class TelegramBotClient {
public:
    static TelegramBotClient& bot() {
        static TelegramBotClient instance;
        return instance;
    }

    void initTelemetryTaskSender();
    ~TelegramBotClient();

    void init();
    bool setBotToken(const String&  botToken) const;
    String getBotToken() const;
    void setChatID (int64_t _chatID);
    int64_t getChatId () const;
    void setPullMode(int pullMode);

    uint32_t sendMessage(const std::string& message);
    void updateMessage(const std::string& message, uint32_t messageId);

    void handleUpdate(fb::Update& u);
    static bool isReadiness();

    // Удаление копирования
    TelegramBotClient(const TelegramBotClient&) = delete;
    TelegramBotClient& operator=(const TelegramBotClient&) = delete;

private:
    TelegramBotClient(); // Приватный конструктор
    SemaphoreHandle_t _botMutex = xSemaphoreCreateMutex();;

    // Добавляем новые приватные поля
    struct CachedMessage {
        std::string header;
        std::string rectificationInfo;
        std::map<std::string, std::vector<std::string>> sensorZones;
        uint32_t lastUpdateTime = 0;
    };

    CachedMessage cachedStatus;
    bool messageStructureInitialized = false;

    void initializeMessageStructure();
    void updateRectificationInfo();
    void updateSensorInfo();

    //
    void sendHello();
    static void statusMessageSender(void* params);
    std::string createStatusMessage();

    FastBot2 _bot;

    int pollMode = 30000;
    String token = "";
    int64_t chatID = 0;

    uint32_t statusMessageID = 0; // ID сообщения статуса

    // В начале файла добавим константы
    static constexpr auto STATUS_HEADER = "👋 <b>Статус телеметрии</b>\n\n";
    static constexpr uint32_t MAX_SEND_TIME_MS = 4000; // Максимум 4 сек на отправку
    const String bootName = "telegram_bot";

};

#endif