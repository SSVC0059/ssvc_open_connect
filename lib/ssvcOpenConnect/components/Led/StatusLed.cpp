#include "StatusLed.h"

/**
*   SSVC Open Connect
 *
 *   A firmware for ESP32 to interface with SSVC 0059 distillation controller
 *   via UART protocol. Features a responsive SvelteKit web interface for
 *   monitoring and controlling the distillation process.
 *   https://github.com/SSVC0059/ssvc_open_connect
 *
 *   Copyright (C) 2024 SSVC Open Connect Contributors
 *
 *   This software is independent and not affiliated with SSVC0059 company.
 *   All Rights Reserved. This software may be modified and distributed under
 *   the terms of the LGPL v3 license. See the LICENSE file for details.
 *
 *   Disclaimer: Use at your own risk. High voltage safety precautions required.
 **/

StatusLed::StatusLed(ESP32SvelteKit *esp32sveltekit)
    : _esp32sveltekit(esp32sveltekit) {}

void StatusLed::begin(uint16_t neoPixelType) {
  _led = new Adafruit_NeoPixel(1, DEFAULT_STATUS_LED_PIN, neoPixelType + NEO_KHZ800);

  _led->begin();
  _led->setBrightness(20);

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
    if (self->_led) {
        ConnectionStatus currentStatus =
            self->_esp32sveltekit->getConnectionStatus();
        ESP_LOGV("StatusLed,", "current status: %d", currentStatus);
        if (currentStatus == ConnectionStatus::OFFLINE) {
        self->_led->fill(0xFF0000);
        } else if (currentStatus == ConnectionStatus::AP) {
        self->_led->fill(0x42AAFF);
        } else if (currentStatus == ConnectionStatus::AP_CONNECTED) {
        self->_led->fill(0x0000FF);
        } else if (currentStatus == ConnectionStatus::STA) {
        self->_led->fill(0xFFFF00);
        } else if (currentStatus == ConnectionStatus::STA_CONNECTED) {
        self->_led->fill(0x00FF00);
        } else {
        self->_led->fill(0xFFFFFF);
        }
        self->_led->show();
    }
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}
