//
// Created by demoncat on 23.12.2024.
//

#include "SsvcOpenConnect.h"

#include <utility>
#include "commons/commons.h"
#include "subsystem/SubsystemManager.h"
#include <subsystem/ThermalSubsystem.h>
#if FT_ENABLED(FT_TELEGRAM_BOT)
#include <subsystem/TelegramBotSubsystem.h>
#endif
#include <ESP32Ping.h>


/**
 * @brief Инициализация модуля SsvcOpenConnect
 */
void SsvcOpenConnect::begin(PsychicHttpServer& server,
                            ESP32SvelteKit& esp32sveltekit,
                            EventSocket* socket,
                            SecurityManager* securityManager)
{
    _server = &server;
    _esp32sveltekit = &esp32sveltekit;
    _socket = socket;
    _securityManager = securityManager;

    // Инициализация сервиса настроек
    _openConnectSettingsService = new OpenConnectSettingsService(_server, _esp32sveltekit, _securityManager);
    _openConnectSettingsService->begin();

    rProcess.begin(
      _ssvcConnector, _ssvcSettings, *_openConnectSettingsService);

    // Инициализация обработчика HTTP запросов
    httpRequestHandler = std::make_unique<HttpRequestHandler>(*_server, _securityManager, rProcess);
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
        result = (http.sendRequest("HEAD") >= 200);  // 2xx или 3xx
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

    // Регистрация подсистем
    ESP_LOGD(TAG, "[SUBSYSTEM_MANAGER] Registering subsystems...");
    SubsystemManager::instance().registerSubsystem<ThermalSubsystem>();
    #if FT_ENABLED(FT_TELEGRAM_BOT)
        SubsystemManager::instance().registerSubsystem<TelegramBotSubsystem>();
    #endif
    ESP_LOGD(TAG, "[SUBSYSTEM_MANAGER] ThermalSubsystem registered");

    // Настройка начальных состояний
    ESP_LOGD(TAG, "[SUBSYSTEM_MANAGER] Setting initial states...");
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

