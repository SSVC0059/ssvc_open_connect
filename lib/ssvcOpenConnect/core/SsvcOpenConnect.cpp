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

#include "SsvcOpenConnect.h"

#if FT_ENABLED(FT_TELEGRAM_BOT)
#include <components/subsystem/TelegramBotSubsystem.h>
#endif
#include <ESP32Ping.h>

#include "AlarmMonitor/Subscribers/NotificationSubscriber.h"

void SsvcOpenConnect::begin(PsychicHttpServer& server,
                            ESP32SvelteKit& esp32sveltekit,
                            EventSocket* socket,
                            SecurityManager* securityManager)
{
    _server = &server;
    _esp32sveltekit = &esp32sveltekit;
    _socket = socket;
    _securityManager = securityManager;

    _openConnectSettingsService = new OpenConnectSettingsService(_server, _esp32sveltekit, _securityManager);
    _openConnectSettingsService->begin();


 // Временный костыль, пока не придумаю как сделать нормально
    _alarmThresholdService = new AlarmThresholdService(_server, _esp32sveltekit);
    _alarmThresholdService->begin();

    _notificationSubscriber = new NotificationSubscriber(_esp32sveltekit);

    AlarmMonitor::getInstance().initialize(_alarmThresholdService);

    _sensorZoneService = new SensorZoneService(_server, _esp32sveltekit);
    _sensorZoneService->begin();
    
    rProcess.begin(
      _ssvcConnector, _ssvcSettings, *_openConnectSettingsService);

    _telemetryService = new TelemetryService(_server, _esp32sveltekit, rProcess);
    _telemetryService->begin();

    httpRequestHandler = std::make_unique<HttpRequestHandler>(*_server, _securityManager);
    httpRequestHandler->begin();

    // Отправка начальных команд
    vTaskDelay(pdMS_TO_TICKS(2000));
    SsvcCommandsQueue::getQueue().getSettings();
    SsvcCommandsQueue::getQueue().version();

    // Инициализация подсистем
    subsystemManager();

}


bool SsvcOpenConnect::isOnline() const
{
    const ConnectionStatus currentStatus =
      _esp32sveltekit->getConnectionStatus();
    bool result = false;
    if (currentStatus == ConnectionStatus::STA ||
        currentStatus == ConnectionStatus::STA_CONNECTED ||
        currentStatus == ConnectionStatus::STA_MQTT)
    {
        HTTPClient http;
        const String url = "http://httpbin.org/get";
        http.begin(url);
        http.setTimeout(3000);
        result = (http.sendRequest("HEAD") >= 200);
        if (result)
        {
            ESP_LOGI("SsvcOpenConnect", "HTTP Response status: %d", result);
        }
        http.end();
        return result;
    }
  return result;
}


void SsvcOpenConnect::subsystemManager()
{
    ESP_LOGI(TAG, "[SUBSYSTEM_MANAGER] Initializing subsystem manager");

    auto& subsystemManager = SubsystemManager::instance();
    ESP_LOGD(TAG, "[SUBSYSTEM_MANAGER] SubsystemManager instance obtained");

    // Запуск менеджера работы с датчиками
    SensorManager manager = SensorManager::getInstance();

    // Регистрация подсистем
    ESP_LOGD(TAG, "[SUBSYSTEM_MANAGER] Registering subsystems...");
    SubsystemManager::instance().registerSubsystem<SettingsSubsystem>();
    SubsystemManager::instance().registerSubsystem<ThermalSubsystem>();
    #if FT_ENABLED(FT_TELEGRAM_BOT)
        SubsystemManager::instance().registerSubsystem<TelegramBotSubsystem>();
    #endif
    ESP_LOGD(TAG, "[SUBSYSTEM_MANAGER] ThermalSubsystem registered");

    // Настройка начальных состояний
    ESP_LOGD(TAG, "[SUBSYSTEM_MANAGER] Setting initial states...");
    subsystemManager.setInitialState("settings", true);
    subsystemManager.setInitialState("thermal", true);

    #if FT_ENABLED(FT_TELEGRAM_BOT)
        subsystemManager.setInitialState("telegram_bot", false);
    #endif

    ESP_LOGD(TAG, "[SUBSYSTEM_MANAGER] thermal subsystem set to enabled by default");

    // Запуск менеджера подсистем
    ESP_LOGD(TAG, "[SUBSYSTEM_MANAGER] Starting subsystem manager...");
    subsystemManager.begin();
    ESP_LOGI(TAG, "[SUBSYSTEM_MANAGER] Initialization complete");
}
