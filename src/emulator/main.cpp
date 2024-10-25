#include <Arduino.h>
#include "JsonSender.h"

const int SERIAL_BAUD_RATE = 115200;
unsigned long previousMillis = 0;
const long interval = 1000;

JsonSender jsonSender;

void setup() {
  Serial.begin(SERIAL_BAUD_RATE);
}

void loop() {
  if (Serial.available()) {
    String request = Serial.readStringUntil('\n');
    if (request == "GET_SETTINGS") {
      jsonSender.sendSettingsResponse();
    }
  }

  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    jsonSender.sendTp1WaitingJson();
  }
}
