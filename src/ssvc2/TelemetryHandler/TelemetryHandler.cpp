#include "TelemetryHandler.h"

TelemetryHandler::TelemetryHandler(AsyncEventSource* eventSource) 
    : _eventSource(eventSource) {}

void TelemetryHandler::sendData(JsonDocument& doc) {
    // Обработка JSON перед отправкой, если это необходимо
    processJson(doc);

    // Преобразование JSON в строку
    String jsonString;
    serializeJson(doc, jsonString);

    // Отправка данных через AsyncEventSource
    _eventSource->send(jsonString.c_str(), nullptr, millis());
}

void TelemetryHandler::processJson(JsonDocument& doc) {

}
