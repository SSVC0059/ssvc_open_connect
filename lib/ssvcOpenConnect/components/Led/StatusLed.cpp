#include "StatusLed.h"

#include <esp_log.h>

namespace {

const char *connectionStatusLabel(ConnectionStatus s) {
  switch (s) {
  case ConnectionStatus::OFFLINE:
    return "OFFLINE";
  case ConnectionStatus::AP:
    return "AP";
  case ConnectionStatus::AP_CONNECTED:
    return "AP_CONNECTED";
  case ConnectionStatus::STA:
    return "STA";
  case ConnectionStatus::STA_CONNECTED:
    return "STA_CONNECTED";
  case ConnectionStatus::STA_MQTT:
    return "STA_MQTT";
  default:
    return "UNKNOWN";
  }
}

} // namespace

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

void StatusLed::refreshLed(const char *phase) {
  if (!_led || !_esp32sveltekit) {
    return;
  }

  const bool uartErr = SsvcConnector::getConnector().uartCommunicationError;
  const ConnectionStatus currentStatus =
      _esp32sveltekit->getConnectionStatus();

  if (uartErr) {
    const bool blinkOn = (millis() / 300) % 2 == 0;
    _led->fill(blinkOn ? 0xFF8000 : 0x000000);
    ESP_LOGI("StatusLed",
             "[%s] UART error -> blink orange (phase=%s) [conn would be %s]",
             phase, blinkOn ? "on" : "off",
             connectionStatusLabel(currentStatus));
  } else {
    const char *mode = "";
    uint32_t rgb = 0;
    if (currentStatus == ConnectionStatus::OFFLINE) {
      rgb = 0xFF0000;
      mode = "red OFFLINE";
    } else if (currentStatus == ConnectionStatus::AP) {
      rgb = 0x42AAFF;
      mode = "cyan AP";
    } else if (currentStatus == ConnectionStatus::AP_CONNECTED) {
      rgb = 0x0000FF;
      mode = "blue AP_CONNECTED";
    } else if (currentStatus == ConnectionStatus::STA) {
      rgb = 0xFFFF00;
      mode = "yellow STA";
    } else if (currentStatus == ConnectionStatus::STA_CONNECTED ||
               currentStatus == ConnectionStatus::STA_MQTT) {
      rgb = 0x00FF00;
      mode = (currentStatus == ConnectionStatus::STA_MQTT) ? "green STA_MQTT"
                                                           : "green STA_CONNECTED";
    } else {
      rgb = 0xFFFFFF;
      mode = "white fallback";
    }
    _led->fill(rgb);
    ESP_LOGI("StatusLed",
             "[%s] connection=%s (%d) -> %s 0x%06lX",
             phase, connectionStatusLabel(currentStatus),
             static_cast<int>(currentStatus), mode,
             static_cast<unsigned long>(rgb));
  }

  _led->show();
}

void StatusLed::begin(uint16_t neoPixelType) {
  if (!_esp32sveltekit) {
    ESP_LOGE("StatusLed", "begin: esp32sveltekit is null");
    return;
  }

  _led = new Adafruit_NeoPixel(1, DEFAULT_STATUS_LED_PIN, neoPixelType + NEO_KHZ800);
  _led->begin();
  _led->setBrightness(20);
  _led->clear();
  _led->show();

  ESP_LOGI("StatusLed",
           "begin: pin=%u type=0x%x stack=4096 core=1",
           static_cast<unsigned>(DEFAULT_STATUS_LED_PIN),
           static_cast<unsigned>(neoPixelType));

  refreshLed("init-sync");

  const BaseType_t taskOk = xTaskCreatePinnedToCore(
      StatusLed::checkStatus,
      "StatusLed",
      4096,
      this,
      (tskIDLE_PRIORITY + 1),
      nullptr,
      1);
  if (taskOk != pdPASS) {
    ESP_LOGE("StatusLed", "xTaskCreatePinnedToCore failed (err=%d)", static_cast<int>(taskOk));
  } else {
    ESP_LOGI("StatusLed", "checkStatus task created");
  }
}

[[noreturn]] void StatusLed::checkStatus(void *pvParameters) {
  auto *self = static_cast<StatusLed *>(pvParameters);
  ESP_LOGI("StatusLed", "checkStatus: task started (self=%p _led=%p svelte=%p)",
           static_cast<void *>(self),
           static_cast<void *>(self ? self->_led : nullptr),
           static_cast<void *>(self ? self->_esp32sveltekit : nullptr));

  if (!self || !self->_led || !self->_esp32sveltekit) {
    ESP_LOGE("StatusLed",
             "checkStatus: invalid self/_led/esp32sveltekit, task idle loop");
    while (true) {
      vTaskDelay(pdMS_TO_TICKS(5000));
    }
  }

  while (true) {
    self->refreshLed("tick");
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
