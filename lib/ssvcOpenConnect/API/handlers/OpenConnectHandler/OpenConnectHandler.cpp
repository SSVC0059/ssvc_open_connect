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

esp_err_t OpenConnectHandler::getInfo(PsychicRequest* request)
{
    auto response = PsychicJsonResponse(request, false);
    const auto root = response.getRoot();

    const SsvcSettings& settings = SsvcSettings::init();

    const auto ssvc = root["ssvc"].to<JsonObject>();
    ssvc["version"] = settings.getSsvcVersion();
    ssvc["api"] = settings.getSsvcApiVersion();
    if (SsvcSettings::init().isSupportTails()) {
        ssvc["mode"] = "tails";
    } else {
        ssvc["mode"] = "late_heads";
    }

    const auto oc = root["oc"].to<JsonObject>();
    oc["version"] = APP_VERSION;
    oc["is_support_api"] = settings.apiSsvcIsSupport();

    response.setCode(200);
    return response.send();
}
