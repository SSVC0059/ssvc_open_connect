/**
 *   ESP32 SvelteKit
 *
 *   A simple, secure and extensible framework for IoT projects for ESP32 platforms
 *   with responsive Sveltekit front-end built with TailwindCSS and DaisyUI.
 *   https://github.com/theelims/ESP32-sveltekit
 *
 *   Copyright (C) 2018 - 2023 rjwats
 *   Copyright (C) 2023 - 2024 theelims
 *
 *   All Rights Reserved. This software may be modified and distributed under
 *   the terms of the LGPL v3 license. See the LICENSE file for details.
 **/

#include <ESP32SvelteKit.h>
#include <PsychicHttpServer.h>
#include <SsvcTelemetryService.h>
#include <RectificationProcess.h>
#include <HttpRequestHandler.h>

// Глобальный мультиплексор для критических секций
portMUX_TYPE ssvcMux = portMUX_INITIALIZER_UNLOCKED;
SemaphoreHandle_t mutex = xSemaphoreCreateMutex();

#define SERIAL_BAUD_RATE 115200

PsychicHttpServer server;

ESP32SvelteKit esp32sveltekit(&server, 120);

EventGroupHandle_t eventGroup = xEventGroupCreate();

SsvcTelemetryService ssvcTelemetryService = SsvcTelemetryService(&server,
                                                                   esp32sveltekit.getSocket(),
                                                                   esp32sveltekit.getSecurityManager(),
                                                                   eventGroup);


// Создаем экземпляр SsvcConnector, передаем созданный eventGroup в конструктор
SsvcConnector& ssvcConnector = SsvcConnector::getConnector(eventGroup);

// Статический экземпляр класса

RectificationProcess& rProcess = RectificationProcess::createRectification(ssvcConnector, eventGroup);
HttpRequestHandler httpRequestHandler = HttpRequestHandler(&server,
                                                           esp32sveltekit.getSecurityManager(),
                                                           rProcess,
                                                           ssvcConnector);

void setup()
{
    // start serial and filesystem
    Serial.begin(SERIAL_BAUD_RATE);

    #ifdef FACTORY_WIFI_HOSTNAME
        esp32sveltekit.setMDNSAppName(FACTORY_WIFI_HOSTNAME);
    #endif
        // start ESP32-SvelteKit
    esp32sveltekit.begin();
    ssvcConnector.begin();
    ssvcTelemetryService.begin();
    
    Serial.println("httpRequestHandler.begin");
    httpRequestHandler.begin();
}

void loop()
{
    vTaskDelete(nullptr);
}
