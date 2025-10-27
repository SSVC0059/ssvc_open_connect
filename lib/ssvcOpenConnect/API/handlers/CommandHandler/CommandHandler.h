#ifndef COMMAND_HANDLER_H
#define COMMAND_HANDLER_H

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
#include "core/SsvcCommandsQueue.h"

class CommandHandler {
public:
    CommandHandler();

    static esp_err_t handleCommand(PsychicRequest* request);

private:
    static const std::map<std::string, std::function<void()>> COMMAND_MAP;
};

#endif
