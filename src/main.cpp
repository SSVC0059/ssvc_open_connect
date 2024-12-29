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
#include <SsvcOpenConnect.h>

// Глобальный мультиплексор для критических секций
portMUX_TYPE ssvcMux = portMUX_INITIALIZER_UNLOCKED;
SemaphoreHandle_t mutex = xSemaphoreCreateMutex();

#define SERIAL_BAUD_RATE 115200

PsychicHttpServer server;

ESP32SvelteKit esp32sveltekit(&server, 120);

SsvcOpenConnect* SsvcOpenConnect::instance = nullptr;

void setup()
{
    SsvcOpenConnect* instance = SsvcOpenConnect::getInstance(server,
                             esp32sveltekit.getSocket(),
                             esp32sveltekit.getSecurityManager());

    // start serial and filesystem
    Serial.begin(SERIAL_BAUD_RATE);

    #ifdef FACTORY_WIFI_HOSTNAME
        esp32sveltekit.setMDNSAppName(FACTORY_WIFI_HOSTNAME);
    #endif
        // start ESP32-SvelteKit
    esp32sveltekit.begin();
    instance->begin();
}

void loop()
{
    vTaskDelete(nullptr);
}
