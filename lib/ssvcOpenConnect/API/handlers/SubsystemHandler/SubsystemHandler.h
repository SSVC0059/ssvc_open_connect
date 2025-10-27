#ifndef SUBSYSTEMHANDLER_H
#define SUBSYSTEMHANDLER_H

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

#include "PsychicHttp.h"

#include "core/SubsystemManager/SubsystemManager.h"
#include <unordered_map>
#include "core/StatefulServices/OpenConnectSettingsService/OpenConnectSettings.h"

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
