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

#include "components/rtc/Ds3231RtcCoordinator.h"
#include "components/sensors/OneWireThermalSubsystem/OneWireThermalSubsystem.h"
#include "components/subsystem/AtmosphericSubsystem.h"
#include "components/subsystem/Lcd1602Subsystem.h"
#include "core/StatefulServices/OpenConnectHardwareSettingsService/OpenConnectHardwareConfig.h"
#include "core/StatefulServices/OpenConnectHardwareSettingsService/OpenConnectHardwareSettingsService.h"
#include "core/RelayRuleEngine/RelayRuleEngine.h"
#include "core/RelayPortCoordinator/RelayPortCoordinator.h"
#include "core/UserRelayProfileBridge/UserRelayProfileBridge.h"
#include "core/SsvcConnector.h"


SsvcOpenConnect& SsvcOpenConnect::getInstance() {
    static SsvcOpenConnect instance;
    return instance;
}

void SsvcOpenConnect::begin(AsyncWebServer& server,
                            ESP32SvelteKit& esp32sveltekit,
                            EventSocket* socket,
                            SecurityManager* securityManager)
{
    ESP_LOGI(TAG, "begin: starting SSVC OpenConnect");
    ESP_LOGD(TAG, "begin: enter (server=%p svelte=%p socket=%p)",
             static_cast<void*>(&server), static_cast<void*>(&esp32sveltekit),
             static_cast<void*>(socket));

    _server = &server;
    _esp32sveltekit = &esp32sveltekit;
    _socket = socket;
    _securityManager = securityManager;
    _mqttClient = _esp32sveltekit->getMqttClient();

    ESP_LOGD(TAG, "begin: StatusLed");
    _statusLed = std::make_unique<StatusLed>(&esp32sveltekit);
    _statusLed->begin(NEO_GRB);

    _profileService = ProfileService::getInstance();

    // Инициализируем сервисы, которые являются наблюдателями, и подписываем их на ProfileService
    ESP_LOGD(TAG, "begin: MQTT/Telegram/Alarm/Sensor services construct");
    _ssvcMqttSettingsService = new SsvcMqttSettingsService(_server, _esp32sveltekit);
    _telegramSettingsService = new TelegramSettingsService(_server, _esp32sveltekit);
    TelegramSettingsService::setInstance(_telegramSettingsService);
#if FT_ENABLED(FT_VK_BOT)
    _vkSettingsService = new VkSettingsService(_server, _esp32sveltekit);
    VkSettingsService::setInstance(_vkSettingsService);
#endif
    _alarmThresholdService = new AlarmThresholdService(_server, _esp32sveltekit);
    _sensorDataService = new SensorDataService(_server, _esp32sveltekit);
    SensorDataService::setInstance(_sensorDataService);
    _sensorConfigService = new SensorConfigService(_server, _esp32sveltekit);

    // Подписываем наблюдателей до вызова _profileService->begin()
    _profileService->subscribe(&_ssvcSettings);
    _profileService->subscribe(_telegramSettingsService);
#if FT_ENABLED(FT_VK_BOT)
    _profileService->subscribe(_vkSettingsService);
#endif
    _profileService->subscribe(&UserRelayProfileBridge::instance());
    // Если _ssvcMqttSettingsService и _alarmThresholdService также являются IProfileObserver,
    // их тоже нужно подписать здесь.
    // _profileService->subscribe(_ssvcMqttSettingsService); // Раскомментировать, если это наблюдатель
    _profileService->subscribe(_alarmThresholdService);

    // Теперь вызываем begin() для ProfileService
    ESP_LOGD(TAG, "begin: ProfileService::begin");
    _profileService->begin(_esp32sveltekit->getFS());

    ESP_LOGD(TAG, "begin: OpenConnectHardwareSettingsService");
    OpenConnectHardwareSettingsService::instance().begin(_esp32sveltekit->getFS());

#if !PINOUT_USE_GPIO
    OpenConnectHardwareSettingsService::instance().read([&](OpenConnectHardwareConfig& cfg) {
        RelayPortCoordinator::getInstance().configure(cfg.relayPcf8574Addresses);
    });
#endif

    // Теперь вызываем begin() для остальных сервисов
    ESP_LOGD(TAG, "begin: Telegram/Alarm/SensorData/SensorConfig begin");
    _telegramSettingsService->begin();
#if FT_ENABLED(FT_VK_BOT)
    _vkSettingsService->begin();
#endif
    _alarmThresholdService->begin();
    _sensorDataService->begin();
    _sensorConfigService->begin();

    ESP_LOGD(TAG, "begin: AlarmMonitor::initialize");
    AlarmMonitor::getInstance().initialize(_alarmThresholdService);

    RelayRuleEngine::getInstance().begin();
    if (!_profileService->applyActiveProfileObservers()) {
        ESP_LOGW(TAG, "applyActiveProfileObservers: skipped or failed (no active profile)");
    }

#if !PINOUT_USE_GPIO
    _esp32sveltekit->getFeatureService()->addFeature(String("openConnectUserRelays"), true);
#endif

    ESP_LOGD(TAG, "begin: SensorCoordinator + OneWire (pin=%u)",
             static_cast<unsigned>(OneWireThermalSubsystem::ONEWIRE_PIN));
    SensorCoordinator::getInstance().registerPollingSubsystem(
        &OneWireThermalSubsystem::getInstance()
    );

    ESP_LOGD(TAG, "begin: SensorCoordinator::startPolling");
    SensorCoordinator::getInstance().startPolling(SENSOR_POLL_INTERVAL_MS);

    _sensorConfigService->addUpdateHandler([&](const String& originId) {
        _sensorDataService->triggerZoneDataRecalculation();
        AlarmMonitor::getInstance().checkAllSensors();
    });

    SensorCoordinator::getInstance().onFirstScanComplete([]() {
        ESP_LOGI("SsvcOpenConnect", "First sensor scan complete. Running initial alarm check.");
        AlarmMonitor::getInstance().checkAllSensors();
    });

    ESP_LOGD(TAG, "begin: Notification + hardware-fault log subscribers");
    _notificationSubscriber = new NotificationSubscriber(_esp32sveltekit);
    _hardwareFaultLogSubscriber = new HardwareFaultLogSubscriber();

    ESP_LOGD(TAG, "begin: RectificationProcess::begin");
    rProcess.begin(
      _ssvcConnector, _ssvcSettings, *_ssvcMqttSettingsService);

    ESP_LOGD(TAG, "begin: TelemetryService");
    _telemetryService = new TelemetryService(_server, _esp32sveltekit, rProcess);
    _telemetryService->begin();

    ESP_LOGD(TAG, "begin: HttpRequestHandler");
    httpRequestHandler = std::make_unique<HttpRequestHandler>(*_server, _securityManager, _profileService, _esp32sveltekit->getFS());
    httpRequestHandler->begin();

    ESP_LOGD(TAG, "begin: delay 2s then SSVC getSettings/version");
    vTaskDelay(pdMS_TO_TICKS(2000));
    const SsvcCommandsQueue* queue = &SsvcCommandsQueue::getQueue();
    queue->getSettings();
    queue->version();

    ESP_LOGD(TAG, "begin: MqttBridge + MqttCommandHandler");
    MqttBridge::getInstance(_esp32sveltekit->getMqttSettingsService());

    const auto commandHandler = new MqttCommandHandler();
    commandHandler->begin();

    ESP_LOGD(TAG, "begin: subsystemManager() (blocks until WiFi)");
    this->subsystemManager();

    ESP_LOGI(TAG, "begin: done");
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
    subsystemManager.registerSingleton<I2CBusSubsystem>();

    bool pressureHardwareOn = true;
    bool lcd1602HardwareOn = false;
    OpenConnectHardwareSettingsService::instance().read([&](OpenConnectHardwareConfig& cfg) {
        pressureHardwareOn = cfg.pressureSensorEnabled;
        lcd1602HardwareOn = cfg.lcd1602Enabled;
    });

    if (pressureHardwareOn) {
        subsystemManager.registerSubsystem<AtmosphericSubsystem>();
    }
    if (lcd1602HardwareOn) {
        subsystemManager.registerSubsystem<Lcd1602Subsystem>();
    }

    #if FT_ENABLED(FT_TELEGRAM_BOT)
        subsystemManager.registerSubsystem<TelegramBotSubsystem>();
    #endif
    #if FT_ENABLED(FT_VK_BOT)
        subsystemManager.registerSubsystem<VkBotSubsystem>();
    #endif
    ESP_LOGD(TAG, "[SUBSYSTEM_MANAGER] Subsystems registered");

    subsystemManager.setInitialState("settings", true);
    subsystemManager.setInitialState("thermal", true);
    subsystemManager.setInitialState("i2c_bus", true);
    if (pressureHardwareOn) {
        subsystemManager.setInitialState("atm_sensor", true);
    }
    if (lcd1602HardwareOn) {
        subsystemManager.setInitialState("lcd1602_display", true);
    }

    // Messengers (telegram_bot, vk_bot): state only from NVS via UI toggle — no setInitialState.

    ESP_LOGD(TAG, "[SUBSYSTEM_MANAGER] Starting subsystem manager...");
    subsystemManager.begin();
    ESP_LOGI(TAG, "[SUBSYSTEM_MANAGER] Initialization complete");

    Ds3231RtcCoordinator::instance().bootstrapAfterI2c();

    {
        SsvcOpenConnect& app = SsvcOpenConnect::getInstance();
        if (app._pinOutSubscriber == nullptr) {
            ESP_LOGI(TAG, "[SUBSYSTEM_MANAGER] PinOutSubscriber (I2C bus ready)");
            app._pinOutSubscriber = new PinOutSubscriber();
        }
    }

    bool ipShow = false;
    while (!ipShow) {
        if (WiFi.isConnected()) {
            ipShow = true;
            SsvcCommandsQueue::getQueue().status( WiFi.localIP().toString().c_str());
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
