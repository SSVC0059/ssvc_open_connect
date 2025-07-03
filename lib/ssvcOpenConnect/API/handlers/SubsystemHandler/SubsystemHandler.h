//
// Created by demoncat on 07.05.25.
//

#ifndef SUBSYSTEMHANDLER_H
#define SUBSYSTEMHANDLER_H

#include "PsychicHttp.h"

#include "core/subsystem/SubsystemManager.h"
#include <unordered_map>
#include "core/OpenConnectSettings/OpenConnectSettings.h"

class SubsystemHandler {
public:
    SubsystemHandler();

    static esp_err_t getStatus(PsychicRequest* request);
    static esp_err_t  state(PsychicRequest* request);
    static esp_err_t  disable(PsychicRequest* request);
    static esp_err_t  enable(PsychicRequest* request);

private:
    static constexpr auto TAG = "SubsystemHandler";
};

#endif //SUBSYSTEMHANDLER_H
