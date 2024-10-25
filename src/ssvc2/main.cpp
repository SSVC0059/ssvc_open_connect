#include <Arduino.h>
#include <WiFiConnector.h>
#include <ssvc2.h>

const int RX_PIN = 16;
const int TX_PIN = 17;

WiFiConnector wifi;
ssvc2 ssvc2Receiver(RX_PIN, TX_PIN, 115200);

void setup() {
    Serial.begin(115200);  // Debug Serial
    ssvc2Receiver.begin();

    // Отправляем команду GET_SETTINGS после запуска
    // ssvc2Receiver.sendGetSettings();
}

void loop() {
    wifi.handleReconnect();
    JsonDocument data;
    if (ssvc2Receiver.receiveData(data)) { // Предполагается, что у вас есть метод для получения данных
        serializeJsonPretty(data, Serial); // Обновляем данные на веб-странице
    }
    delay(1000);
}
