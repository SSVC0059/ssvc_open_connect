#ifndef TELEMETRY_HANDLER_H
#define TELEMETRY_HANDLER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <AsyncEventSource.h>

class TelemetryHandler {
public:
    TelemetryHandler(AsyncEventSource* eventSource);
    void sendData(JsonDocument& doc);
    void processJson(JsonDocument& doc);

private:
    AsyncEventSource* _eventSource;
};

#endif // TELEMETRY_HANDLER_H
