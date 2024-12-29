//
// Created by demoncat on 03.12.2024.
//

#ifndef SSVC_OPEN_CONNECT_HTTPREQUESTHANDLER_H
#define SSVC_OPEN_CONNECT_HTTPREQUESTHANDLER_H


#include <time.h>
#include <WiFi.h>
#include <lwip/apps/sntp.h>

#include <ArduinoJson.h>
#include <PsychicHttp.h>
#include <SecurityManager.h>
#include <RectificationProcess.h>
#include <ArduinoJson.h>

#define GET_SETTINGS_ROUTE "/rest/getSettings"
#define SEND_COMMAND_ROUTE "/rest/ssvcCommands"
#define TEMP_METRICS_DATA_ROUTE "/rest/tMetrics"


class HttpRequestHandler {
public:
    HttpRequestHandler(PsychicHttpServer& server,
                       SecurityManager *securityManager,
                       RectificationProcess& rProcess,
                       SsvcConnector& ssvcConnector);

    void begin();

private:
    PsychicHttpServer& _server;
    SecurityManager* _securityManager;
    RectificationProcess& _rProcess;
    SsvcConnector& _ssvcConnector;

    esp_err_t reqStatus(PsychicRequest *request);
    esp_err_t postCommandStatusStatus(PsychicRequest *request);
    esp_err_t tMetrixResponce(PsychicRequest *request);
};

#endif //SSVC_OPEN_CONNECT_HTTPREQUESTHANDLER_H
