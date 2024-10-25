#ifndef JSONSENDER_H
#define JSONSENDER_H

#include <Arduino.h>
#include <ArduinoJson.h>

class JsonSender {
public:
    JsonSender();

    // Методы для отправки JSON данных
    void sendCommonJson();
    void sendTp1WaitingJson();
    void sendSettingsResponse();

private:
    int tp1Value = 25;  // Начальное значение tp1 для sendTp1WaitingJson
};

#endif // JSONSENDER_H
