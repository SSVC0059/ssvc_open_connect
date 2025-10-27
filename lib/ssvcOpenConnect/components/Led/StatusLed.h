#ifndef SSVC_OPEN_CONNECT_STATUS_LED_H
#define SSVC_OPEN_CONNECT_STATUS_LED_H

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
