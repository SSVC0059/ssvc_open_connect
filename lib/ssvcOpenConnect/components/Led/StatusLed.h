//
// Created by demoncat on 12.03.2025.
//

#ifndef SSVC_OPEN_CONNECT_STATUS_LED_H
#define SSVC_OPEN_CONNECT_STATUS_LED_H

#include "Adafruit_NeoPixel.h"
#include "ESP32SvelteKit.h"

#define DEFAULT_STATUS_LED_PIN 48

class StatusLed {

public:
  explicit StatusLed(ESP32SvelteKit *esp32sveltekit);
  void begin();

private:
  uint32_t currentColor{};
  ESP32SvelteKit *_esp32sveltekit;

  [[noreturn]] static void checkStatus(void *pvParameters);
};

#endif // SSVC_OPEN_CONNECT_STATUS_LED_H
