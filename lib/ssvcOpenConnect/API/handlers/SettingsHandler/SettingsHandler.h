//
// Created by demoncat on 05.05.25.
//

#ifndef SETTINGS_HANDLER_H
#define SETTINGS_HANDLER_H

#include "PsychicHttp.h"
#include "core/SsvcSettings/SettingsSetterHandlers.h"
#include "core/SsvcCommandsQueue.h"
#include <vector>
#include <utility>


class SettingsHandler {
public:
    SettingsHandler();

    static esp_err_t getSettings(PsychicRequest* request);
    static esp_err_t updateSettings(PsychicRequest* request);

private:
    static void parseQueryParams(const String& query,
                               std::vector<std::pair<String, String>>& output);
};

#endif