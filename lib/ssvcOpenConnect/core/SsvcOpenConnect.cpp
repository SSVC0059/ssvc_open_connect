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
#include "components/sensors/SensorCoordinator/SensorCoordinator.h"
#include "external/MqttBridge/MqttBridge.h"
#include "MqttCommandHandler/MqttCommandHandler.h"
#include "StatefulServices/SensorDataService/SensorDataService.h"

SsvcOpenConnect& SsvcOpenConnect::getInstance() {
    static SsvcOpenConnect instance;
    return instance;
}

void SsvcOpenConnect::begin(PsychicHttpServer& server,
                            ESP32SvelteKit& esp32sveltekit,
                            EventSocket* socket,
                            SecurityManager* securityManager)
{
    _server = &server;
    _esp32sveltekit = &esp32sveltekit;
    _socket = socket;
    _securityManager = securityManager;
    _mqttClient = _esp32sveltekit->getMqttClient();

    _profileService = ProfileService::getInstance();

    // Инициализируем сервисы, которые являются наблюдателями, и подписываем их на ProfileService
    _ssvcMqttSettingsService = new SsvcMqttSettingsService(_server, _esp32sveltekit);
    _telegramSettingsService = new TelegramSettingsService(_server, _esp32sveltekit);
    TelegramSettingsService::setInstance(_telegramSettingsService);
    _alarmThresholdService = new AlarmThresholdService(_server, _esp32sveltekit);
    _sensorDataService = new SensorDataService(_server, _esp32sveltekit);
    SensorDataService::setInstance(_sensorDataService);
    _sensorConfigService = new SensorConfigService(_server, _esp32sveltekit);

    // Подписываем наблюдателей до вызова _profileService->begin()
    _profileService->subscribe(&_ssvcSettings);
    _profileService->subscribe(_telegramSettingsService);
    // Если _ssvcMqttSettingsService и _alarmThresholdService также являются IProfileObserver,
    // их тоже нужно подписать здесь.
    // _profileService->subscribe(_ssvcMqttSettingsService); // Раскомментировать, если это наблюдатель
    // _profileService->subscribe(_alarmThresholdService); // Раскомментировать, если это наблюдатель

    // Теперь вызываем begin() для ProfileService
    _profileService->begin(_esp32sveltekit->getFS());

    // Теперь вызываем begin() для остальных сервисов
    _telegramSettingsService->begin();
    _alarmThresholdService->begin();
    _sensorDataService->begin();
    _sensorConfigService->begin();
    _sensorConfigService->addUpdateHandler([&](const String& originId) {
        _sensorDataService->triggerZoneDataRecalculation();
    });

    SensorCoordinator::getInstance().registerPollingSubsystem(
        &OneWireThermalSubsystem::getInstance()
    );
    SensorCoordinator::getInstance().startPolling(SENSOR_POLL_INTERVAL_MS);

    _notificationSubscriber = new NotificationSubscriber(_esp32sveltekit);

    AlarmMonitor::getInstance().initialize(_alarmThresholdService);

    rProcess.begin(
      _ssvcConnector, _ssvcSettings, *_ssvcMqttSettingsService);

    _telemetryService = new TelemetryService(_server, _esp32sveltekit, rProcess);
    _telemetryService->begin();

    httpRequestHandler = std::make_unique<HttpRequestHandler>(*_server, _securityManager, _profileService, _esp32sveltekit->getFS());
    httpRequestHandler->begin();

    vTaskDelay(pdMS_TO_TICKS(2000));
    const SsvcCommandsQueue* queue = &SsvcCommandsQueue::getQueue();
    queue->getSettings();
    queue->version();

    MqttBridge::getInstance(_esp32sveltekit->getMqttSettingsService());

    const auto commandHandler = new MqttCommandHandler();
    commandHandler->begin();

    this->subsystemManager();

    // Инициализация StatusLed
    _statusLed = std::make_unique<StatusLed>(&esp32sveltekit);
    //  uint16_t neoPixelType = _globalSettings.getNeoPixelType();
    _statusLed->begin(NEO_GRB);

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

void SsvcOpenConnect::sendHello() {
    SsvcCommandsQueue::getQueue().status("Привет!");
    const std::string version = SsvcSettings::init().getSsvcVersion();
    SsvcCommandsQueue::getQueue().status(std::string("SSVC: ") + version);
    const float versionApi = SsvcSettings::init().getSsvcApiVersion();
    SsvcCommandsQueue::getQueue().status((String("API: ") + versionApi).c_str());
    SsvcCommandsQueue::getQueue().status("OpenConnect");
    const std::string versionOC = APP_VERSION;
    SsvcCommandsQueue::getQueue().status("v:  " + versionOC);
}

void SsvcOpenConnect::subsystemManager()
{
    ESP_LOGI(TAG, "[SUBSYSTEM_MANAGER] Initializing subsystem manager");

    auto& subsystemManager = SubsystemManager::instance();
    ESP_LOGD(TAG, "[SUBSYSTEM_MANAGER] SubsystemManager instance obtained");

    subsystemManager.registerSubsystem<SettingsSubsystem>();
    subsystemManager.registerSubsystem<ThermalSubsystem>();

    #if FT_ENABLED(FT_TELEGRAM_BOT)
        subsystemManager.registerSubsystem<TelegramBotSubsystem>();
    #endif
    ESP_LOGD(TAG, "[SUBSYSTEM_MANAGER] Subsystems registered");

    subsystemManager.setInitialState("settings", true);
    subsystemManager.setInitialState("thermal", true);

    #if FT_ENABLED(FT_TELEGRAM_BOT)
        subsystemManager.setInitialState("telegram_bot", false);
    #endif

    ESP_LOGD(TAG, "[SUBSYSTEM_MANAGER] Starting subsystem manager...");
    subsystemManager.begin();
    ESP_LOGI(TAG, "[SUBSYSTEM_MANAGER] Initialization complete");

    bool ipShow = false;
    while (!ipShow) {
        if (WiFi.isConnected()) {
            ipShow = true;
            SsvcCommandsQueue::getQueue().status( WiFi.localIP().toString().c_str());
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
