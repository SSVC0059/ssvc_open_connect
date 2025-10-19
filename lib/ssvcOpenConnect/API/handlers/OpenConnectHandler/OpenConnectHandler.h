//
// Created by Admin on 13/10/2025.
//

#ifndef SSVC_OPEN_CONNECT_OPENCONNECTHANDLER_H
#define SSVC_OPEN_CONNECT_OPENCONNECTHANDLER_H

#include "PsychicHttp.h"
#include <utility>

class OpenConnectHandler
{
    public:
        OpenConnectHandler();
        static esp_err_t getInfo(PsychicRequest* request);
};



#endif //SSVC_OPEN_CONNECT_OPENCONNECTHANDLER_H