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

#include <Arduino.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "ESP32SvelteKit.h"

#include "API/HttpRequestHandler.h"
#include "components/Led/StatusLed.h"
#include "core/MqttCommandHandler/MqttCommandHandler.h"
#include "core/SsvcCommandsQueue.h"
#include "core/profiles/ProfileService.h"
#include "external/MqttBridge/MqttBridge.h"

SsvcOpenConnect& SsvcOpenConnect::getInstance() {
    static SsvcOpenConnect instance;
    return instance;
}

SsvcOpenConnect::~SsvcOpenConnect() = default;

void SsvcOpenConnect::begin(AsyncWebServer& server,
                            ESP32SvelteKit& esp32sveltekit,
                            EventSocket* socket,
                            SecurityManager* securityManager)
{
    ESP_LOGI(TAG, "begin: starting SSVC Open Connect");

    initPlatform(server, esp32sveltekit, socket, securityManager);

    ESP_LOGD(TAG, "begin: service registry (services, observers, sensors, alarms)");
    _services.begin(_ctx);

    ESP_LOGD(TAG, "begin: HttpRequestHandler");
    initHttp();

    ESP_LOGD(TAG, "begin: delay 2s then SSVC getSettings/version + MQTT");
    initMessaging();

    ESP_LOGD(TAG, "begin: subsystemManager() (blocks until WiFi)");
    _subsystems.start(_services);

    ESP_LOGI(TAG, "begin: done");
}

void SsvcOpenConnect::initPlatform(AsyncWebServer& server,
                                   ESP32SvelteKit& esp32sveltekit,
                                   EventSocket* socket,
                                   SecurityManager* securityManager)
{
    // EventSocket* socket is part of the public begin() contract but not
    // consumed by the current boot phases — kept for API compatibility.
    (void)socket;

    _ctx = AppContext(&server, &esp32sveltekit, securityManager);

    ESP_LOGD(TAG, "begin: StatusLed");
    _statusLed = std::unique_ptr<StatusLed>(new StatusLed(&esp32sveltekit));
    _statusLed->begin(NEO_GRB);
}

void SsvcOpenConnect::initHttp()
{
    _http = std::unique_ptr<HttpRequestHandler>(
        new HttpRequestHandler(*_ctx.server, _ctx.securityManager,
                               ProfileService::getInstance(), _ctx.sveltekit->getFS()));
    _http->begin();
}

void SsvcOpenConnect::initMessaging()
{
    vTaskDelay(pdMS_TO_TICKS(2000));
    const SsvcCommandsQueue* queue = &SsvcCommandsQueue::getQueue();
    queue->getSettings();
    queue->version();

    ESP_LOGD(TAG, "begin: MqttBridge + MqttCommandHandler");
    MqttBridge::getInstance(_ctx.sveltekit->getMqttSettingsService());

    _mqttCommands = std::unique_ptr<MqttCommandHandler>(new MqttCommandHandler());
    _mqttCommands->begin();
}
