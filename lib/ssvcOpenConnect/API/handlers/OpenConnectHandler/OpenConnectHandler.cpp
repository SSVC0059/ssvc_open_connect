#include "OpenConnectHandler.h"

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

#include "core/SsvcOpenConnect.h"
#include "core/SsvcSettings/SsvcSettings.h"

OpenConnectHandler::OpenConnectHandler() = default;

void OpenConnectHandler::getInfo(AsyncWebServerRequest* request)
{
    AsyncJsonResponse* response = new AsyncJsonResponse();
    JsonObject root = response->getRoot();

    const SsvcSettings& settings = SsvcSettings::init();

    JsonObject ssvc = root["ssvc"].to<JsonObject>();
    ssvc["version"] = settings.getSsvcVersion().c_str();
    ssvc["api"] = settings.getSsvcApiVersion();
    ssvc["mode"] = SsvcSettings::init().isSupportTails() ? "tails" : "late_heads";

    JsonObject oc = root["oc"].to<JsonObject>();
    oc["version"] = APP_VERSION;
    oc["is_support_api"] = settings.apiSsvcIsSupport();

    response->setLength();
    request->send(response);
}
