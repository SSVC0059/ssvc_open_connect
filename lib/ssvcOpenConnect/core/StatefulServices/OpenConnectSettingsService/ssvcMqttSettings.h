#ifndef SSVC_OPEN_CONNECT_OPENCONNECTSETTINGS_H
#define SSVC_OPEN_CONNECT_OPENCONNECTSETTINGS_H

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

#include "ArduinoJson.h"
#include "ESP32SvelteKit.h"
#include "StatefulService.h"
#include "core/SsvcSettings/SsvcSettings.h"
// #include "core/profiles/IProfileObserver.h" // Remove this include

#define OPEN_CONNECT_SETTINGS_PUB_TOPIC "openconnect/settings"

class ssvcMqttSettings
{
public:
    static void read(const ssvcMqttSettings& settings, const JsonObject& root);
    static StateUpdateResult update(JsonObject& root, ssvcMqttSettings& state) {
        return StateUpdateResult::UNCHANGED;
    }
};

class SsvcMqttSettingsService : public StatefulService<ssvcMqttSettings>
{
public:

    SsvcMqttSettingsService(PsychicHttpServer* server,
                               ESP32SvelteKit* _esp32sveltekit);

    ~SsvcMqttSettingsService() = default;

    static SsvcMqttSettingsService* getInstance() {return _instance;}

private:
    static SsvcMqttSettingsService* _instance;
    MqttEndpoint<ssvcMqttSettings> _mqttEndpoint;

};


#endif // SSVC_OPEN_CONNECT_OPENCONNECTSETTINGS_H
