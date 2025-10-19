//
// Created by demoncat on 12.03.2025.
//

#include "StatusLed.h"

Adafruit_NeoPixel led(1, DEFAULT_STATUS_LED_PIN, NEO_GRB + NEO_KHZ800);

StatusLed::StatusLed(ESP32SvelteKit *esp32sveltekit)
    : _esp32sveltekit(esp32sveltekit) {}

void StatusLed::begin() {

  led.begin();
  led.setBrightness(20);

  //    Запуск задачи отслеживания текущего статуса подключения

  ESP_LOGV("SsvcConnector", "start _telemetry task");

  // Запуск сбора телеметрии
  xTaskCreatePinnedToCore(
      StatusLed::checkStatus, // Function that should be called
      "StatusLed",            // Name of the task (for debugging)
      2048,                   // Stack size (bytes)
      this,                   // Pass reference to this class instance
      (tskIDLE_PRIORITY),     // task priority
      nullptr,                // Task handle
      1                       // Pin to application core
  );
}

[[noreturn]] void StatusLed::checkStatus(void *pvParameters) {
  auto *self = static_cast<StatusLed *>(pvParameters);
  while (true) {
    ConnectionStatus currentStatus =
        self->_esp32sveltekit->getConnectionStatus();
    ESP_LOGV("StatusLed,", "current status: %d", currentStatus);
    if (currentStatus == ConnectionStatus::OFFLINE) {
      led.fill(0xFF0000);
    } else if (currentStatus == ConnectionStatus::AP) {
      led.fill(0x42AAFF);
    } else if (currentStatus == ConnectionStatus::AP_CONNECTED) {
      led.fill(0x0000FF);
    } else if (currentStatus == ConnectionStatus::STA) {
      led.fill(0xFFFF00);
    } else if (currentStatus == ConnectionStatus::STA_CONNECTED) {
      led.fill(0x00FF00);
    } else {
      led.fill(0xFFFFFF);
    }
    led.show();
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}
