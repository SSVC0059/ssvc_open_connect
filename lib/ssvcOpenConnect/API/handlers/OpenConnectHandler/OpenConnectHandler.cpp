//
// Created by Admin on 13/10/2025.
//

#include "OpenConnectHandler.h"

#include "core/SsvcOpenConnect.h"
#include "core/SsvcSettings/SsvcSettings.h"

OpenConnectHandler::OpenConnectHandler() = default;

esp_err_t OpenConnectHandler::getInfo(PsychicRequest* request)
{
    auto response = PsychicJsonResponse(request, false);
    const auto root = response.getRoot();

    const auto ssvc = root["ssvc"].to<JsonObject>();
    ssvc["version"] = SsvcSettings::init().getSsvcVersion();
    ssvc["api"] = SsvcSettings::init().getSsvcApiVersion();

    const auto oc = root["oc"].to<JsonObject>();
    oc["version"] = APP_VERSION;

    response.setCode(200);
    return response.send();
}
