#ifndef WIFICONNECTOR_H
#define WIFICONNECTOR_H

#include <WiFiManager.h>  // Подключаем библиотеку WiFiManager

class WiFiConnector {
public:
    // Конструктор
    WiFiConnector();

    // Метод для начала подключения к Wi-Fi
    void begin();

    // Метод для проверки статуса соединения
    bool isConnected();
    
    // Метод для управления переподключением
    void handleReconnect();

private:
    WiFiManager wifiManager;
};

#endif  // WIFICONNECTOR_H
