//
// Created by demoncat on 03.12.2024.
//

#ifndef SSVC_OPEN_CONNECT_HTTPREQUESTHANDLER_H
#define SSVC_OPEN_CONNECT_HTTPREQUESTHANDLER_H


#include <Arduino.h>
#include <ArduinoJson.h>
#include <PsychicHttp.h>
#include <SecurityManager.h>
#include <RectificationProcess.h>

#define GET_STATUS_ROUTE "/rest/status"
#define GET_SETTINGS_ROUTE "/rest/settings"
#define SEND_COMMAND_ROUTE "/rest/commands"
#define TEMP_METRICS_DATA_ROUTE "/rest/metrics"


class HttpRequestHandler {
public:
    HttpRequestHandler(PsychicHttpServer& server,
                       SecurityManager *securityManager,
                       RectificationProcess& rProcess,
                       SsvcConnector& ssvcConnector,
                       SsvcSettings& ssvcSettings);

    void begin();

private:
    PsychicHttpServer& _server;
    SecurityManager* _securityManager;
    RectificationProcess& _rProcess;
    SsvcConnector& _ssvcConnector;
    SsvcSettings& _ssvcSettings;

    esp_err_t getStatus(PsychicRequest *request);
    esp_err_t getSettings(PsychicRequest *request);
    esp_err_t postCommandStatusStatus(PsychicRequest *request);
    esp_err_t tMetrixResponce(PsychicRequest *request);
};

#endif //SSVC_OPEN_CONNECT_HTTPREQUESTHANDLER_H
