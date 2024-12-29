//
// Created by demoncat on 23.12.2024.
//

#include "SsvcOpenConnect.h"


SsvcOpenConnect::SsvcOpenConnect(PsychicHttpServer& server,
                                 EventSocket *socket,
                                 SecurityManager* securityManager) : _server(server),
                                                                     _socket(socket),
                                                                     _securityManager(securityManager),
                                                                     _eventGroup(xEventGroupCreate()),

                                                                     _ssvcConnector(SsvcConnector::getConnector(_eventGroup)),
                                                                     rProcess(
                                                                                RectificationProcess::createRectification(
                                                                                    _ssvcConnector,
                                                                                    _eventGroup)
                                                                             ),
                                                                     _ssvcTelemetryService(
                                                                             SsvcTelemetryService(
                                                                                     _socket,
                                                                                    _securityManager,
                                                                                    _eventGroup,
                                                                                    &rProcess)
                                                                            ),
                                                                     httpRequestHandler(
                                                                             HttpRequestHandler(server,
                                                                                   _securityManager,
                                                                                   rProcess,
                                                                                   _ssvcConnector)
                                                                             )
{

}

void SsvcOpenConnect::begin() {
    _ssvcConnector.begin();
    _ssvcTelemetryService.begin();
    httpRequestHandler.begin();
}

