//
// Created by demoncat on 23.12.2024.
//

#include <EventSocket.h>
#include <HttpEndpoint.h>
#include <SecurityManager.h>
#include <SsvcTelemetryService.h>
#include <SsvcConnector.h>
#include <HttpRequestHandler.h>
#include <RectificationProcess.h>

#ifndef SSVC_OPEN_CONNECT_SSVCOPENCONNECT_H
#define SSVC_OPEN_CONNECT_SSVCOPENCONNECT_H


class SsvcOpenConnect {
    public:
        static SsvcOpenConnect* getInstance(PsychicHttpServer& server,
                                            EventSocket* socket,
                                            SecurityManager* securityManager) {
            if (!instance) {
                instance = new SsvcOpenConnect(server,
                                               socket,
                                               securityManager);
            }
            return instance;
        }

        void begin();

    private:
        SsvcOpenConnect(PsychicHttpServer& server,
                        EventSocket* socket,
                        SecurityManager* securityManager);

        PsychicHttpServer& _server;
        EventSocket *_socket;
        SecurityManager* _securityManager;
        EventGroupHandle_t _eventGroup;

        SsvcTelemetryService _ssvcTelemetryService;
        SsvcConnector& _ssvcConnector;
        RectificationProcess& rProcess;
        HttpRequestHandler httpRequestHandler;


    // Указатель на единственный экземпляр класса
        static SsvcOpenConnect* instance;
};






#endif //SSVC_OPEN_CONNECT_SSVCOPENCONNECT_H
