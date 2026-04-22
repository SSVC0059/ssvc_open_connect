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

#include "SsvcConnector.h"
#include "SsvcOpenConnect.h"
#include "core/AlarmMonitor/AlarmMonitor.h"

#include <esp_intr_alloc.h>

// Инициализация статической переменной
SsvcConnector *SsvcConnector::_ssvcConnector = nullptr;

SsvcConnector::SsvcConnector() {
  uartCommunicationError = false;
  lastSettings = "";
  lastMessage = "";
  InitUartDriver();
}

SsvcConnector &SsvcConnector::getConnector() {
  if (!_ssvcConnector) {
    _ssvcConnector = new SsvcConnector();
  }
  return *_ssvcConnector; // Возвращаем указатель на экземпляр
}

void SsvcConnector::InitUartDriver() {
#if BOARD_KINCONY_KC868_A6
  constexpr int kUartIntrFlags = ESP_INTR_FLAG_LOWMED;
  const char *kBoardTag = "KC868-A6";
#else
  constexpr int kUartIntrFlags = ESP_INTR_FLAG_LEVEL3;
  const char *kBoardTag = "default";
#endif
  constexpr uint32_t kRxRingBytes = SSVC_OPEN_CONNECT_BUF_SIZE * 2;
  constexpr int kTxBuf = 0;
  constexpr int kQueueSize = 0;

  ESP_LOGI("SsvcConnector",
           "InitUartDriver: start board=%s uart=%d TX=%d RX=%d link=TTL/RS485-transceiver",
           kBoardTag, static_cast<int>(SSVC_OPEN_CONNECT_UART_NUM),
           static_cast<int>(SSVC_OPEN_CONNECT_UART_TX),
           static_cast<int>(SSVC_OPEN_CONNECT_UART_RX));
  ESP_LOGI(
      "SsvcConnector",
      "InitUartDriver: buffers rx_ring=%u tx_driver=%d event_queue=%d "
      "uart_intr_flags=0x%x (%s)",
      static_cast<unsigned>(kRxRingBytes), kTxBuf, kQueueSize, kUartIntrFlags,
#if BOARD_KINCONY_KC868_A6
      "LOWMED (WiFi/PHY friendly)"
#else
      "LEVEL3"
#endif
  );

  // Idle TTL RX high helps when the line would otherwise float before the transceiver drives it.
  ESP_LOGD("SsvcConnector", "InitUartDriver: gpio_set_pull_mode(RX=%d, PULLUP_ONLY)",
           static_cast<int>(SSVC_OPEN_CONNECT_UART_RX));
  gpio_set_pull_mode(SSVC_OPEN_CONNECT_UART_RX, GPIO_PULLUP_ONLY);

  constexpr uart_config_t uart_config = {.baud_rate = 115200,
                                         .data_bits = UART_DATA_8_BITS,
                                         .parity = UART_PARITY_DISABLE,
                                         .stop_bits = UART_STOP_BITS_1,
                                         .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
                                         .rx_flow_ctrl_thresh = 0,
                                         .source_clk = UART_SCLK_APB};
  ESP_LOGI("SsvcConnector",
           "InitUartDriver: uart_param_config baud=%d data=%d parity=%d stop=%d "
           "flow=%d rx_thresh=%d src_clk=%d (APB)",
           uart_config.baud_rate, static_cast<int>(uart_config.data_bits),
           static_cast<int>(uart_config.parity),
           static_cast<int>(uart_config.stop_bits),
           static_cast<int>(uart_config.flow_ctrl),
           static_cast<int>(uart_config.rx_flow_ctrl_thresh),
           static_cast<int>(uart_config.source_clk));

  esp_err_t ret = uart_param_config(SSVC_OPEN_CONNECT_UART_NUM, &uart_config);
  if (ret != ESP_OK) {
    ESP_LOGE("SsvcConnector",
             "InitUartDriver: uart_param_config FAILED uart=%d err=%s (0x%x)",
             static_cast<int>(SSVC_OPEN_CONNECT_UART_NUM), esp_err_to_name(ret),
             static_cast<unsigned>(ret));
    return;
  }
  ESP_LOGI("SsvcConnector", "InitUartDriver: uart_param_config OK");

  ESP_LOGI("SsvcConnector",
           "InitUartDriver: uart_set_pin tx=%d rx=%d rts=%d cts=%d (NO_CHANGE=-1)",
           static_cast<int>(SSVC_OPEN_CONNECT_UART_TX),
           static_cast<int>(SSVC_OPEN_CONNECT_UART_RX), UART_PIN_NO_CHANGE,
           UART_PIN_NO_CHANGE);
  ret = uart_set_pin(SSVC_OPEN_CONNECT_UART_NUM, SSVC_OPEN_CONNECT_UART_TX,
                     SSVC_OPEN_CONNECT_UART_RX, UART_PIN_NO_CHANGE,
                     UART_PIN_NO_CHANGE);
  if (ret != ESP_OK) {
    ESP_LOGE("SsvcConnector",
             "InitUartDriver: uart_set_pin FAILED err=%s (0x%x)", esp_err_to_name(ret),
             static_cast<unsigned>(ret));
    return;
  }
  ESP_LOGI("SsvcConnector", "InitUartDriver: uart_set_pin OK");

  ESP_LOGD("SsvcConnector", "InitUartDriver: gpio_set_pull_mode(TX=%d, PULLUP_ONLY)",
           static_cast<int>(SSVC_OPEN_CONNECT_UART_TX));
  gpio_set_pull_mode(SSVC_OPEN_CONNECT_UART_TX, GPIO_PULLUP_ONLY);

  // KinCony + RS485: default intr allocation coexists better with WiFi/PHY during boot than
  // high-priority LEVEL3 (reduces TG1WDT / IRQ storms when RX is noisy or WiFi is calibrating).

  ESP_LOGI("SsvcConnector",
           "InitUartDriver: uart_driver_install(rx=%u tx=%d queue=%d queue_hdl=null "
           "intr_flags=0x%x)",
           static_cast<unsigned>(kRxRingBytes), kTxBuf, kQueueSize, kUartIntrFlags);
  ret = uart_driver_install(SSVC_OPEN_CONNECT_UART_NUM, kRxRingBytes, kTxBuf,
                            kQueueSize, nullptr, kUartIntrFlags);
  if (ret != ESP_OK) {
    ESP_LOGE("SsvcConnector",
             "InitUartDriver: uart_driver_install FAILED err=%s (0x%x)",
             esp_err_to_name(ret), static_cast<unsigned>(ret));
    return;
  }
  ESP_LOGI("SsvcConnector", "InitUartDriver: uart_driver_install OK");

  uart_flush_input(SSVC_OPEN_CONNECT_UART_NUM);
  ESP_LOGI("SsvcConnector",
           "InitUartDriver: uart_flush_input done (RX FIFO cleared), driver ready");

#if BOARD_KINCONY_KC868_A6
  // Let RF/PHY and RS485 transceiver settle before the telemetry task polls RX (avoids early WDT).
  constexpr uint32_t kPostUartSettleMs = 150;
  ESP_LOGI("SsvcConnector",
           "InitUartDriver: vTaskDelay(%ums) for PHY/RS485 settle", static_cast<unsigned>(kPostUartSettleMs));
  vTaskDelay(pdMS_TO_TICKS(kPostUartSettleMs));
  ESP_LOGI("SsvcConnector", "InitUartDriver: settle delay done");
#endif

  ESP_LOGI("SsvcConnector", "InitUartDriver: complete, calling initSsvcController()");
  ESP_LOGV("SsvcConnector", "Starting loop task");

  SsvcConnector::initSsvcController();
}

// CRON
void SsvcConnector::initSsvcController() {
  ESP_LOGV("SsvcConnector", "start _telemetry task");
  //     Запуск сбора телеметрии
  xTaskCreatePinnedToCore(
      SsvcConnector::_telemetry, // Function that should be called
      "TelemetryTask",           // Name of the task (for debugging)
      4096,                      // Stack size (bytes)
      this,                      // Pass reference to this class instance
      (tskIDLE_PRIORITY + 1),    // task priority
      nullptr,                   // Task handle
      1                          // Pin to application core
  );
}

constexpr uint32_t UART_READ_TIMEOUT_MS = 5000;

[[noreturn]] void SsvcConnector::_telemetry(void *pvParameters) {
  auto *self = static_cast<SsvcConnector *>(pvParameters);
  static char data[1024];        // Буфер для данных
  int errorCounter = 0;

  while (true) {
    size_t data_len = 0;
    uart_get_buffered_data_len(SSVC_OPEN_CONNECT_UART_NUM, &data_len);
    if (data_len > SSVC_OPEN_CONNECT_BUF_SIZE) {
      ESP_LOGE("SsvcConnector", "Buffer overflow detected, clearing buffer!");
      uart_flush(SSVC_OPEN_CONNECT_UART_NUM); // Очистка буфера
    }

    memset(data, 0, sizeof(data));
    int idx = 0;
    TickType_t lastByteTime = xTaskGetTickCount();
    bool lineReadTimedOut = false;

    while (true) {
      // Чтение одного байта за раз
      const int len = uart_read_bytes(SSVC_OPEN_CONNECT_UART_NUM,
                                (uint8_t *)&data[idx], 1, pdMS_TO_TICKS(100));
      if (len > 0) {
        lastByteTime = xTaskGetTickCount();
        // Если встретили символ переноса строки, завершаем чтение
        if (data[idx] == '\n') {
          data[idx] = '\0'; // Завершаем строку
          break;
        }
        idx++;
        // Проверка на переполнение буфера
        if (idx >= sizeof(data) - 1) {
          data[sizeof(data) - 1] = '\0'; // Ограничиваем строку
          break;
        }
      } else {
        // Таймаут: если долго нет данных — выходим (SSVC выключен)
        if ((xTaskGetTickCount() - lastByteTime) > pdMS_TO_TICKS(UART_READ_TIMEOUT_MS)) {
          lineReadTimedOut = true;
          break;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
      }
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, data);
    if (error) {
      // Порог — по *подряд* идущим ошибкам; успешный кадр сбрасывает счётчик ниже.
      // Таймаут чтения строки без единого байта (тишина на RX) учитывается сильнее.
      constexpr int errorThreshold = 5;
      if (lineReadTimedOut && idx == 0) {
        errorCounter += 2;
      } else {
        errorCounter++;
      }
      ESP_LOGE("SsvcConnector", "Ошибка десериализации: %s", error.c_str());

      if (errorCounter >= errorThreshold) {
        if (!self->uartCommunicationError) {
          ESP_LOGW("SsvcConnector",
                   "uartCommunicationError=true (errors=%d, threshold=%d)",
                   errorCounter, errorThreshold);
          SsvcCommandsQueue::getQueue().scheduleUartRetryTimer();
          AlarmMonitor::getInstance().raiseHardwareFault(
              HardwareFaultCode::UART_LINK_LOST, 0, "ssvc_uart");
        }
        self->uartCommunicationError = true;
      }
    } else {
      errorCounter = 0;

      ESP_LOGV("SsvcConnector", "Начало вывода данных");
      ESP_LOGV("SsvcConnector", "%s", data);
      ESP_LOGV("SsvcConnector", "Конец вывода данных");

      if (xSemaphoreTake(mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        self->lastMessage = std::string(data);
        ESP_LOGV("SsvcConnector", "lastMessage: %s", self->lastMessage.c_str());
        xSemaphoreGive(mutex); // Копирование данных
      } else {
        ESP_LOGE("SsvcConnector",
                 "Не удалось захватить мьютекс для lastMessage");
      }

      // SSVC появился после ошибки связи — запрашиваем настройки и версию
      if (self->uartCommunicationError) {
        ESP_LOGI("SsvcConnector", "SSVC connected, requesting settings and version");
        SsvcCommandsQueue::getQueue().getSettings();
        SsvcCommandsQueue::getQueue().version();
        AlarmMonitor::getInstance().clearHardwareFault("ssvc_uart", 0);
      }
      self->uartCommunicationError = false;
      if (doc["type"] == "response") {
        if (doc["request"] == "GET_SETTINGS") {
          ESP_LOGV("SsvcConnector", "GET_SETTINGS: SEND BIT10");
          xEventGroupSetBits(eventGroup, BIT10);
        } if (doc["request"] == "VERSION") {
          ESP_LOGV("SsvcConnector", "result: SEND BIT11 start");
          ESP_LOGV("SsvcConnector", "END BIT11 lastMessage: %s",
                   self->lastMessage.c_str());
          self->lastResponse = std::string(data);
          xEventGroupSetBits(eventGroup, BIT11);
          ESP_LOGV("SsvcConnector", "result: SEND BIT11 -end");

        } else if (doc["result"] == "OK") {
          ESP_LOGV("SsvcConnector", "result: SEND BIT9 start");
          ESP_LOGV("SsvcConnector", "END BIT9 lastMessage: %s",
                   self->lastMessage.c_str());
          self->lastResponse = std::string(data);
          xEventGroupSetBits(eventGroup, BIT9);
          ESP_LOGV("SsvcConnector", "result: SEND BIT9 -end");
        } else {
          ESP_LOGV("SsvcConnector", "ERROR send command");
          xEventGroupSetBits(eventGroup, BIT1);
        }
        // Отправка ответа на MQTT
        (void)MqttBridge::getInstance().publish(
          MQTT_RSP_TOPIC,
          data,
          1,
          true);
      } else {
        if (doc["common"]["cfg_chgd"]) {
          ESP_LOGV("SsvcConnector",
                   "Изменены настройки SSVC на устройстве кнопками");
          SsvcCommandsQueue::getQueue().getSettings();
        }
        // Телеметрия и всё остальное
        xEventGroupSetBits(eventGroup, BIT0);
      }
    }

    vTaskDelay(pdMS_TO_TICKS(500)); 
  }
}

bool SsvcConnector::sendCommand(const char *command) {
  char *commandCopy = strdup(command);
  if (!commandCopy) {
    ESP_LOGE("SsvcConnector", "Ошибка копирования команды");
    return false;
  }
  ESP_LOGD("SsvcConnector", "Отправка команды SSVC: %s", commandCopy);
  ESP_LOGD("SsvcConnector", "strlen: %d", strlen(commandCopy));
  const ssize_t bytes_written = uart_write_bytes(
      SSVC_OPEN_CONNECT_UART_NUM, (const char *)commandCopy, strlen(commandCopy));
  const bool result = (bytes_written == strlen(commandCopy));
  ESP_LOGD("sendCommand", "result: %s", result ? "true" : "false");
  uart_wait_tx_done(SSVC_OPEN_CONNECT_UART_NUM, pdMS_TO_TICKS(1000));
  free(commandCopy);
  return result;
}

std::string SsvcConnector::getLastMessage() { return lastMessage; }

std::string SsvcConnector::getLastResponse() { return lastResponse; }
