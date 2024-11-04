#include <Arduino.h>
#include <WiFiManager.h>
#include "header.h"
#include "./Connector/Connector.h"
#include "Logger.h"
#include <ESPAsyncWebServer.h>
#include "./TelemetryHandler/TelemetryHandler.h"


Connector* connector;
Logger logger; // Создаем объект Logger, задача для логирования будет запущена
AsyncWebServer server(80);
AsyncEventSource events("/events");
TelemetryHandler telemetryHandler(&events);

void setup() {
    Serial.begin(115200);
       //WiFiManager, Local intialization. Once its business is done, there is no need to keep it around
    WiFiManager wm;
    bool is_connected;
    is_connected = wm.autoConnect("Open_Connect"); // password protected ap

    if(!is_connected) {
        Serial.println("Failed to connect");
        // ESP.restart();
    } 
    else {
        //if you get here you have connected to the WiFi    
        Serial.println("connected...yeey :)");
    }

    Logger::setLogLevel(LOG_LEVEL_DEBUG); // Устанавливаем уровень логирования

    connector = new Connector(
        UART_NUM_1,
        GPIO_NUM_17,
        GPIO_NUM_16
    ); 
    connector->start();

    connector->sendCommandGetSettings();

    // Определение маршрута для корневого URL
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        // HTML с эмуляцией SSE на клиенте
        request->send(200, "text/html", R"rawliteral(
            <!DOCTYPE html>
            <html>
            <head>
                <title>SSE Example</title>
            </head>
            <body>
                <h1>Server-Sent Events (SSE) Example</h1>
                <p id="sse-data">Waiting for events...</p>
                <script>
                    const eventSource = new EventSource('/events');
                    eventSource.onmessage = function(event) {
                        const sseData = document.getElementById('sse-data');
                        sseData.innerHTML = event.data;
                    };
                </script>
            </body>
            </html>
        )rawliteral");
    });

    /// Инициализация события
    server.addHandler(&events);
    server.begin();


}

void loop() {
    // Основной цикл остается пустым, поскольку задачи работают асинхронно
}

