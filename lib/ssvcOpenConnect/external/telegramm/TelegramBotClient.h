#ifndef TELEGRAM_BOT_CLIENT_H
#define TELEGRAM_BOT_CLIENT_H

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

#include <functional>
#include <unordered_map>
#include <iomanip>
#include <string_view>

#include <WiFi.h>
#include <FastBot2.h>

#include "core/SsvcOpenConnect.h"
#include "core/GlobalConfig/GlobalConfig.h"
#include <components/subsystem/ThermalSubsystem.h>
#include "commons/commons.h"

#define FB_USE_LOG Serial
#define MESSAGE_LIMIT_BY_UPDATE 3

class TelegramBotClient {
public:
    static TelegramBotClient& bot() {
        static TelegramBotClient instance;
        return instance;
    }

    void initTelemetryTaskSender();
    ~TelegramBotClient();
    TaskHandle_t _telemetryTaskHandle = nullptr;

    bool init();
    void shutoff();
    [[nodiscard]] bool setBotToken(const String&  botToken) const;
    [[nodiscard]] String getBotToken() const;
    [[nodiscard]] String createControlKeyboard() const;

    void setChatID (int64_t _chatID);
    [[nodiscard]] int64_t getChatId () const;
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

    //
    void sendHello();
    static void statusMessageSender(void* params);
    std::string createStatusMessage();

    FastBot2 _bot;

    int pollMode = 30000;
    String token = "";
    int64_t chatID = 0;

    uint32_t statusMessageID = 0;

    // В начале файла добавим константы
    static constexpr auto STATUS_HEADER = "👋 <b>Статус телеметрии</b>\n\n";
    static constexpr uint32_t MAX_SEND_TIME_MS = 4000; // Максимум 4 сек на отправку
    const String bootName = "telegram_bot";

};

#endif