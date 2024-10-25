#include "ssvc2.h"
#include <HardwareSerial.h>


// ssvc2::ssvc2(int rxPin, int txPin, int baudRate)
//     : rxPin(rxPin), txPin(txPin), baudRate(baudRate) {}

ssvc2::ssvc2(int rxPin, int txPin, int baudRate)
    : rxPin(rxPin), txPin(txPin), baudRate(baudRate)
{
    inputBuffer.reserve(256);
}

void ssvc2::begin()
{
    Serial2.begin(baudRate, SERIAL_8N1, rxPin, txPin); // Инициализация Serial2 с 8N1
}

// Чтение данных из Serial2
bool ssvc2::readData() {
    while (Serial2.available()) {
        char ch = Serial2.read();
        if (ch == '\n') {  // Проверка окончания строки
            DeserializationError error = deserializeJson(jsonDoc, receivedBuffer);
            inputBuffer = ""; // Очистка буфера после обработки
            if (!error) {
                return true; // Возвращаем true, если JSON успешно десериализован
            } else {
                Serial.println("Ошибка десериализации JSON: " + String(error.c_str()));
            }

            if (!error) {
                return true;  // Успешно десериализован JSON
            } else {
                Serial.println("JSON parse failed");
                return false;
            }
        } else {
            receivedBuffer += ch;
        }
    }
    return false;
}

bool ssvc2::receiveData(JsonDocument& doc) {
    while (Serial2.available()) {
        char c = Serial2.read(); // Чтение символа из Serial2
        if (c == '\n') { // Проверка конца строки
            processInput(); // Обработка входящих данных
            // Попытка десериализации JSON
            DeserializationError error = deserializeJson(doc, inputBuffer);
            inputBuffer = ""; // Очистка буфера после обработки
            if (!error) {
                return true; // Возвращаем true, если JSON успешно десериализован
            } else {
                Serial.println("Ошибка десериализации JSON: " + String(error.c_str()));
            }
        } else {
            inputBuffer += c; // Добавление символа в буфер
        }
    }
    return false; // Возвращаем false, если данных нет
}

void ssvc2::processInput() {
    // Здесь можно добавить дополнительную логику для обработки входящих данных,
    // если это необходимо.
}

// Отправка команды по Serial2
void ssvc2::sendCommand(const String& command) {
    Serial2.print(command + "\n");
}

// Специальный метод для отправки команды GET_SETTINGS
void ssvc2::sendGetSettings() {
    sendCommand("GET_SETTINGS");
}

// Специальный метод для отправки команды AT
void ssvc2::sendATCommand() {
    sendCommand("AT");
}

// Возвращаем последний полученный JSON объект
JsonDocument& ssvc2::getReceivedData() {
    return jsonDoc;
}
