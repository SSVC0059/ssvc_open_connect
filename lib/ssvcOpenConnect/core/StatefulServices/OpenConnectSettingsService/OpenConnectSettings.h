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

#define OPEN_CONNECT_SETTINGS_PUB_TOPIC "openconnect/settings"
#define OPEN_CONNECT_SETTINGS_ENDPOINT_PATH "/rest/oc_settings"

class OpenConnectSettings
{
public:
    static void read(const OpenConnectSettings& settings, const JsonObject& root);
    static StateUpdateResult update(JsonObject& root, OpenConnectSettings& state) {
        return StateUpdateResult::UNCHANGED;
    }
};

class OpenConnectSettingsService : public StatefulService<OpenConnectSettings>
{
public:

    OpenConnectSettingsService(PsychicHttpServer* server,
                               ESP32SvelteKit* _esp32sveltekit);

    ~OpenConnectSettingsService() = default;

    static OpenConnectSettingsService* getInstance() {return _instance;}

    void begin();

private:
    static OpenConnectSettingsService* _instance;
    HttpEndpoint<OpenConnectSettings> _httpEndpoint;
    MqttEndpoint<OpenConnectSettings> _mqttEndpoint;


};


#endif // SSVC_OPEN_CONNECT_OPENCONNECTSETTINGS_H
