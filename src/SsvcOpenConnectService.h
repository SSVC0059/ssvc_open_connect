#pragma once

#include <EventSocket.h>
#include <HttpEndpoint.h>
#include <MqttEndpoint.h>
#include <EventEndpoint.h>
#include <WebSocketServer.h>
#include <Arduino.h>
#include <driver/uart.h>
#include <StatefulService.h>

#define SSVC_OPEN_CONNECT_ENDPOINT_PATH "/rest/openConnect"
#define SSVC_OPEN_CONNECT_SOCKET_PATH "/ws/openConnect"
#define SSVC_OPEN_CONNECT_EVENT "openConnect"

// UART CONFIGS

#define SSVC_OPEN_CONNECT_UART_NUM UART_NUM_1
#define SSVC_OPEN_CONNECT_UART_TX GPIO_NUM_17
#define SSVC_OPEN_CONNECT_UART_RX GPIO_NUM_16
#define SSVC_OPEN_CONNECT_BUF_SIZE 2048

#define SSVC_OPEN_CONNECT_QUEUE_LENGTH 10
#define SSVC_OPEN_CONNECT_QUEUE_TIMEOUT 1000
#define SSVC_OPEN_CONNECT_QUEUE_SIZE 1024

class SsvcOpenConnect {

    char telemetryData[1024];

    public:

        static void read(SsvcOpenConnect &SsvcOpenConnect, JsonObject &root);

        static StateUpdateResult update(JsonObject &root, SsvcOpenConnect &SsvcOpenConnect);

        static void homeAssistRead(SsvcOpenConnect &SsvcOpenConnect, JsonObject &root)
        {
            Serial.println("SsvcOpenConnect::homeAssistRead");
        }

        static StateUpdateResult homeAssistUpdate(JsonObject &root, SsvcOpenConnect &SsvcOpenConnect)
        {
            Serial.println("SsvcOpenConnect::homeAssistUpdate");
            return StateUpdateResult::UNCHANGED;
        }


    private:
        static StateUpdateResult commandHandler(const String &_command);

        static void sendCommand(const char *command) {
            Serial.println("Sending command: " + String(command));
            uart_write_bytes(UART_NUM_1, command, strlen(command));
        }
    
};

class SsvcOpenConnectService : public StatefulService<SsvcOpenConnect>
{
public:
    SsvcOpenConnectService(PsychicHttpServer *server,
                      EventSocket *socket,
                      SecurityManager *securityManager,
                      PsychicMqttClient *mqttClient);

    void begin();


private:
    EventSocket *_socket;
    TaskFunction_t _loop() const;

    HttpEndpoint<SsvcOpenConnect> _httpEndpoint;
    EventEndpoint<SsvcOpenConnect> _eventEndpoint;
    MqttEndpoint<SsvcOpenConnect> _mqttEndpoint;
    WebSocketServer<SsvcOpenConnect> _webSocketServer;
    PsychicMqttClient *_mqttClient;


    void uartInitializing();
    void onTelemetryUpdated();

};
