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

// UART CONFIGS
//
// Physical link can be:
// - Direct TTL UART (legacy): ESP TX/RX ↔ SSVC UART.
// - RS485 (typical with KinCony KC868-A6): ESP TTL UART ↔ MAX13487 (or external RS485↔TTL) ↔
//   twisted pair ↔ (SSVC side: RS485↔TTL) ↔ SSVC. Auto-direction transceivers (e.g. MAX13487E)
//   do not expose DE/RE to the MCU — firmware keeps standard async UART; no uart_set_mode(RS485).
// See docs/boards/kc868-a6/GPIO_Pinout.md — KC868-A6 v6: RS485 TTL TX=GPIO17 RX=GPIO18 (build_flags).

#define SSVC_OPEN_CONNECT_UART_NUM UART_NUM_1
#ifndef SSVC_OPEN_CONNECT_UART_TX
#define SSVC_OPEN_CONNECT_UART_TX GPIO_NUM_17
#endif
#ifndef SSVC_OPEN_CONNECT_UART_RX
#define SSVC_OPEN_CONNECT_UART_RX GPIO_NUM_18
#endif

#ifndef BOARD_KINCONY_KC868_A6
#define BOARD_KINCONY_KC868_A6 0
#endif

/**
 * 1 = alarm line outputs on GPIO only (independent of PCF8574).
 * 0 = alarm lines driven via I2C PCF8574 (see SSVC_RELAY_PCF8574_*); mutually exclusive compile paths.
 * Set per board in platformio.ini — do not infer from BOARD_* here.
 */
#ifndef PINOUT_USE_GPIO
#define PINOUT_USE_GPIO 1
#endif

/** GPIO alarm outputs when PINOUT_USE_GPIO=1; must not clash with other firmware uses (I2C/UART/etc.). */
#ifndef SSVC_PINOUT_ALARM_GPIO_DANGEROUS
#define SSVC_PINOUT_ALARM_GPIO_DANGEROUS GPIO_NUM_8
#endif
#ifndef SSVC_PINOUT_ALARM_GPIO_CRITICAL
#define SSVC_PINOUT_ALARM_GPIO_CRITICAL GPIO_NUM_13
#endif

/** First (or only) PCF8574 relay/output port when PINOUT_USE_GPIO=0. Extra chips: another port + role. */
#ifndef SSVC_RELAY_PCF8574_I2C_ADDR
#define SSVC_RELAY_PCF8574_I2C_ADDR 0x24
#endif
#ifndef SSVC_RELAY_PCF8574_DEVICE_ROLE
#define SSVC_RELAY_PCF8574_DEVICE_ROLE "pcf8574_port0"
#endif
#ifndef SSVC_RELAY_PCF8574_BIT_DANGEROUS
#define SSVC_RELAY_PCF8574_BIT_DANGEROUS 0
#endif
#ifndef SSVC_RELAY_PCF8574_BIT_CRITICAL
#define SSVC_RELAY_PCF8574_BIT_CRITICAL 1
#endif
/** Outputs per PCF8574 (fixed by chip); several expanders = several ports / roles / addresses. */
#ifndef SSVC_RELAY_PCF8574_LINES_PER_CHIP
#define SSVC_RELAY_PCF8574_LINES_PER_CHIP 8
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
