#ifndef SSVC2_H
#define SSVC2_H

#include <Arduino.h>
#include <HardwareSerial.h>
#include <ArduinoJson.h>

class ssvc2 {
public:
    ssvc2(int rxPin, int txPin, int baudRate);
    
    void begin();                // Инициализация Serial2
    bool receiveData(JsonDocument& doc); 
    bool readData();             // Чтение данных из Serial2 и анализ
    void sendCommand(const String& command);  // Отправка команды
    JsonDocument& getReceivedData();  // Возврат полученных данных

    // Методы для отправки определенных команд
    void sendGetSettings();
    void sendATCommand();

private:
    int rxPin;          // Пин для приёма данных
    int txPin;          // Пин для передачи данных
    long baudRate;      // Скорость передачи данных
    String inputBuffer; // Буфер для хранения входящих данных
    JsonDocument jsonDoc;  // Для хранения последнего полученного JSON
    String receivedBuffer;            // Буфер для чтения строк из Serial2
    void processInput(); // Метод для обработки входящих данных
};

#endif
