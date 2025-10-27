#ifndef SSVCCONNECTOR_H
#define SSVCCONNECTOR_H

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


#include "ArduinoJson.h"
#include "driver/gpio.h"
#include <Arduino.h>
#include <driver/uart.h>
#include <iostream>

#include <algorithm>
#include <sstream>
#include <utility>
#include <vector>

// Определение минимальной поддерживаемой версии
#define SSVC_MIN_SUPPORT_VERSION "2.2.38"

// UART CONFIGS

#define SSVC_OPEN_CONNECT_UART_NUM UART_NUM_1
#ifndef SSVC_OPEN_CONNECT_UART_TX
#define SSVC_OPEN_CONNECT_UART_TX GPIO_NUM_17
#endif
#ifndef SSVC_OPEN_CONNECT_UART_RX
#define SSVC_OPEN_CONNECT_UART_RX GPIO_NUM_16
#endif

constexpr size_t SSVC_OPEN_CONNECT_BUF_SIZE = 4096;

extern SemaphoreHandle_t mutex;
extern EventGroupHandle_t eventGroup;

class SsvcConnector {
public:
  static SsvcConnector &getConnector();

  std::string getLastMessage();

  std::string getLastResponse();

  bool uartCommunicationError;

  static bool sendCommand(const char *command);

private:
  explicit SsvcConnector();

  void InitUartDriver();
  void initSsvcController();

  [[noreturn]] static void _telemetry(void *pvParameters);

  static SsvcConnector *_ssvcConnector;
  QueueHandle_t command_queue{};

  std::string lastMessage;
  std::string lastSettings;
  std::string lastResponse;

  struct SpiRamAllocator final : ArduinoJson::Allocator {
    virtual ~SpiRamAllocator() = default;

    void *allocate(const size_t size) override {
      return heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
    }

    void deallocate(void *pointer) override { heap_caps_free(pointer); }

    void *reallocate(void *ptr, const size_t new_size) override {
      return heap_caps_realloc(ptr, new_size, MALLOC_CAP_SPIRAM);
    }
  };
};

#endif
