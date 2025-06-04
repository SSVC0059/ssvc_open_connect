//
// Created by demoncat on 05.05.25.
//

#ifndef COMMAND_HANDLER_H
#define COMMAND_HANDLER_H

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
