#include "WiFiConnector.h"
#include <Arduino.h>

WiFiConnector::WiFiConnector() {
    // Настройки точки доступа (AP), если Wi-Fi не подключен
    wifiManager.setAPCallback([](WiFiManager *myWiFiManager) {
        Serial.println("Включен режим точки доступа");
    });
}

void WiFiConnector::begin() {
    Serial.println("Подключение к Wi-Fi...");

    // Пытаемся подключиться к последней сети, либо включаем точку доступа
    if (!wifiManager.autoConnect("ESP32_AP")) {
        Serial.println("Не удалось подключиться и точка доступа не была активирована");
        delay(3000);
        ESP.restart();  // Перезагружаем контроллер для повтора
    }

    Serial.println("Подключение к Wi-Fi успешно!");
}

bool WiFiConnector::isConnected() {
    return WiFi.status() == WL_CONNECTED;
}

void WiFiConnector::handleReconnect() {
    if (!isConnected()) {
        Serial.println("Wi-Fi отключён, попытка переподключения...");
        wifiManager.autoConnect();
    }
}
