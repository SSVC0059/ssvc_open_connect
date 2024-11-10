#include <Arduino.h>
#include <WiFiManager.h>
#include "header.h"
#include "./Connector/Connector.h"
#include "Logger.h"
#include <ESPAsyncWebServer.h>
#include "./TelemetryHandler/TelemetryHandler.h"
#include "svelteesp32.h"


Connector* connector;
Logger logger; // Создаем объект Logger, задача для логирования будет запущена
AsyncWebServer server(80);
AsyncEventSource events("/events");
TelemetryHandler telemetryHandler(&events);


void handleExecuteCommand(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
  JsonDocument doc;
  JsonDocument responce;
  DeserializationError error = deserializeJson(doc, data);

  if (error) {
    Serial.println("Failed to parse JSON");
    request->send(400, "application/json", "{\"status\":\"failed\", \"reason\":\"invalid JSON\"}");
    return;
  }

  // Проверка наличия поля commands
  if (doc.containsKey("command")) {
    String command = doc["command"].as<String>();

    // В зависимости от команды выполняем разные действия
     // В зависимости от команды выполняем разные действия
    if (command == "GET_SETTINGS") {
        connector->sendCommandGetSettings();
    } 
    else if (command == "STOP") {
        connector->sendCommandStop();
    } 
    else if (command == "PAUSE") {
        connector->sendCommandPause();
    } 
    else if (command == "NEXT") {
        connector->sendCommandNext();
    } 
    else if (command == "VERSION") {
        connector->sendCommandGetVersion();
    } 
    else {
        // Если команда не распознана, возвращаем ошибку
        request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Unknown command\"}");
    }

    responce["status"] = "success";
    responce["command"] = command;
    responce["state"] = "OK";

    String response;
    serializeJson(responce, response);
    request->send(200, "application/json", response);

  } else {
    request->send(400, "application/json", "{\"status\":\"failed\", \"reason\":\"missing command\"}");
  }
}



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

    /// Инициализация события
    server.addHandler(&events);
    initSvelteStaticFiles(&server);


    // Определение маршрута для POST запроса
    server.on("/command", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL, handleExecuteCommand);


    server.begin();


}

void loop() {
    // Основной цикл остается пустым, поскольку задачи работают асинхронно
}

