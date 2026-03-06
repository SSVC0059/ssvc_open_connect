#ifndef TELEGRAM_BOT_CLIENT_H
#define TELEGRAM_BOT_CLIENT_H

#include <functional>
#include <unordered_map>
#include <iomanip>
#include <string_view>

#include <WiFi.h>
#include <FastBot2.h>

#include "core/SsvcOpenConnect.h"
#include "core/StatefulServices/TelegramSettingsService/TelegramSettingsService.h"
#include <components/subsystem/ThermalSubsystem.h>
#include "commons/commons.h"

#define FB_USE_LOG Serial
#define MESSAGE_LIMIT_BY_UPDATE 3

class TelegramBotClient {
public:
    static TelegramBotClient& bot();

    void initTelemetryTaskSender();
    ~TelegramBotClient();
    TaskHandle_t _telemetryTaskHandle = nullptr;

    bool init(TelegramSettingsService* settingsService);
    void shutoff();
    void setBotToken(const String&  botToken);
    String getBotToken() const;
    String createControlKeyboard() const;

    void setChatID (int64_t _chatID);
    int64_t getChatId () const;
    void setPullMode(int pullMode);

    uint32_t sendMessage(const std::string& message);
    void updateMessage(const std::string& message, uint32_t messageId);

    static bool isReadiness();

    TelegramBotClient(const TelegramBotClient&) = delete;
    TelegramBotClient& operator=(const TelegramBotClient&) = delete;

private:
    TelegramBotClient();
    SemaphoreHandle_t _botMutex = xSemaphoreCreateMutex();

    bool _initialized = false;
    TelegramSettingsService* _settingsService = nullptr;

    struct CachedMessage {
        std::string header;
        std::string rectificationInfo;
        std::map<std::string, std::vector<std::string>> sensorZones;
        uint32_t lastUpdateTime = 0;
        bool showControlButtons = false;
    };

    CachedMessage cachedStatus;
    bool messageStructureInitialized = false;

    void initializeMessageStructure();
    void updateRectificationInfo();
    void updateSensorInfo();

    void sendHello();
    static void statusMessageSender(void* params);
    std::string createStatusMessage();

    FastBot2 _bot;

    int pollMode = 30000;
    String token = "";
    int64_t chatID = 0;

    uint32_t statusMessageID = 0;

    static constexpr auto STATUS_HEADER = "👋 <b>Статус телеметрии</b>\n\n";
    static constexpr uint32_t MAX_SEND_TIME_MS = 4000;
    const String bootName = "telegram_bot";
};

#endif
