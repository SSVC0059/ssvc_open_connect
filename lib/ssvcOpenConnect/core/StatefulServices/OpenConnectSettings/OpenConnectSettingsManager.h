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

#include "ESP32SvelteKit.h"

#define OPEN_CONNECT_SETTINGS_ENDPOINT_PATH "/rest/oc_settings"
#define OPEN_CONNECT_SETTINGS_FILE "/config/Settings.json"

class OpenConnectSettingsManager
{
public:
    int pid;
    std::string sensorsConfig;

    static void read(const OpenConnectSettingsManager& settings, const JsonObject& root);
    static StateUpdateResult update(const JsonObject& root, OpenConnectSettingsManager& settings);

};

class OpenConnectSettingsService : public StatefulService<OpenConnectSettingsManager>
{
public:
    OpenConnectSettingsService(PsychicHttpServer* server,
                               ESP32SvelteKit* _esp32sveltekit,
                               SecurityManager* _securityManager);

    ~OpenConnectSettingsService();

    void begin();

private:
    HttpEndpoint<OpenConnectSettingsManager> _httpEndpoint;
    FSPersistence<OpenConnectSettingsManager> _fsPersistence;

    static void onConfigUpdated();
};


#endif // SSVC_OPEN_CONNECT_OPENCONNECTSETTINGS_H
