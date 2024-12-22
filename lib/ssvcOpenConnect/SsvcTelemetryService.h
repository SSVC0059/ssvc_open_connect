#pragma once

#include "EventSocket.h"
#include "HttpEndpoint.h"
#include "MqttEndpoint.h"
#include "EventEndpoint.h"
#include "WebSocketServer.h"
#include <Arduino.h>
#include <driver/uart.h>
#include "StatefulService.h"
#include "SsvcConnector.h"
#include "RectificationProcess.h"

#define SSVC_OPEN_CONNECT_ENDPOINT_PATH "/rest/openConnect"
#define SSVC_OPEN_CONNECT_SOCKET_PATH "/ws/openConnect"
#define SSVC_OPEN_CONNECT_EVENT "openConnect"

// UART CONFIGS

#define SSVC_OPEN_CONNECT_UART_NUM UART_NUM_1
#ifndef SSVC_OPEN_CONNECT_UART_TX
    #define SSVC_OPEN_CONNECT_UART_TX GPIO_NUM_17
#endif
#ifndef SSVC_OPEN_CONNECT_UART_RX
    #define SSVC_OPEN_CONNECT_UART_RX GPIO_NUM_16
#endif
#define SSVC_OPEN_CONNECT_BUF_SIZE 2048


extern RectificationProcess& rProcess;

class SsvcTelemetry {

    char telemetryData[1024];

    public:

        static void read(SsvcTelemetry &SsvcOpenConnect, JsonObject &root);

        static StateUpdateResult update(JsonObject &root, SsvcTelemetry &SsvcOpenConnect);

        static void homeAssistRead(SsvcTelemetry &SsvcOpenConnect, JsonObject &root)
        {
            Serial.println("SsvcTelemetry::homeAssistRead");
        }

        static StateUpdateResult homeAssistUpdate(JsonObject &root, SsvcTelemetry &SsvcOpenConnect)
        {
            Serial.println("SsvcTelemetry::homeAssistUpdate");
            return StateUpdateResult::UNCHANGED;
        }

    private:

};

class SsvcTelemetryService : public StatefulService<SsvcTelemetry>
{
public:
    SsvcTelemetryService(PsychicHttpServer *server,
                         EventSocket *socket,
                         SecurityManager *securityManager,
                         EventGroupHandle_t _eventGroup);

    void begin();


private:
    EventSocket *_socket;
    // Переменная для хранения eventGroup
    EventGroupHandle_t _eventGroup;

    static void _telemetry(void* pvParameters);

    HttpEndpoint<SsvcTelemetry> _httpEndpoint;
    EventEndpoint<SsvcTelemetry> _eventEndpoint;
    WebSocketServer<SsvcTelemetry> _webSocketServer;
    PsychicMqttClient *_mqttClient;
    RectificationProcess* rectificationProcess;

    void onTelemetryUpdated();

};
