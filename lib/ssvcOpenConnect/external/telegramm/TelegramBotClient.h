#ifndef TELEGRAM_BOT_CLIENT_H
#define TELEGRAM_BOT_CLIENT_H

#include <functional>
#include <unordered_map>
#include <iomanip>
#include <string_view>

#include <WiFi.h>
#include <FastBot2.h>

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

    using BootDoneFn = void (*)(bool ok, void* ctx);

    bool init(TelegramSettingsService* settingsService);
    bool startBoot(TelegramSettingsService* settingsService, BootDoneFn onDone = nullptr,
                   void* onDoneCtx = nullptr);
    bool isBootInProgress() const { return _bootTaskHandle != nullptr; }
    void shutoff();
    void setBotToken(const String&  botToken);
    String getBotToken() const;
    String createControlKeyboard() const;

    void setChatID(const String& _chatID);
    String getChatId() const;
    void setPullMode(int pullMode);

    uint32_t sendMessage(const std::string& message);
    void updateMessage(const std::string& message, uint32_t messageId);

    // Возвращает true, если Telegram Bot API доступен (проверяется при init).
    static bool isReadiness();
    bool isApiReady() const { return _apiReady; }

    TelegramBotClient(const TelegramBotClient&) = delete;
    TelegramBotClient& operator=(const TelegramBotClient&) = delete;

private:
    TelegramBotClient();
    SemaphoreHandle_t _botMutex = xSemaphoreCreateMutex();

    bool _initialized   = false;
    bool _apiReady      = false;
    volatile bool _bootAbort = false;
    TaskHandle_t _bootTaskHandle = nullptr;
    BootDoneFn _bootDoneFn = nullptr;
    void* _bootDoneCtx = nullptr;
    TelegramSettingsService* _settingsService = nullptr;

    bool runBootSequence();
    bool probeTelegramApi();
    static void bootTaskEntry(void* param);
    void finishBoot(bool ok);

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
    String chatID = "";

    uint32_t statusMessageID = 0;

    static constexpr auto STATUS_HEADER = "👋 <b>Статус телеметрии</b>\n\n";
    static constexpr uint32_t MAX_SEND_TIME_MS = 4000;
    const String bootName = "telegram_bot";
};

#endif
