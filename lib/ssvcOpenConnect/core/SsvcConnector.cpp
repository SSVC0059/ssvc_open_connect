//
// Created by demoncat on 26.11.2024.
//

#include "SsvcConnector.h"
#include "SsvcOpenConnect.h"

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
  ESP_LOGI("SsvcConnector", "Start SsvcConnector begin()");
  gpio_set_pull_mode(SSVC_OPEN_CONNECT_UART_RX, GPIO_PULLUP_ONLY);

  constexpr uart_config_t uart_config = {.baud_rate = 115200,
                                         .data_bits = UART_DATA_8_BITS,
                                         .parity = UART_PARITY_DISABLE,
                                         .stop_bits = UART_STOP_BITS_1,
                                         .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
                                         .rx_flow_ctrl_thresh = 0,
                                         .source_clk = UART_SCLK_APB};
  esp_err_t ret;
  ret = uart_param_config(SSVC_OPEN_CONNECT_UART_NUM, &uart_config);
  if (ret != ESP_OK) {
    ESP_LOGE("SsvcConnector", "uart_param_config failed: %s",
             esp_err_to_name(ret));
    return;
  }
  ret = uart_set_pin(SSVC_OPEN_CONNECT_UART_NUM, SSVC_OPEN_CONNECT_UART_TX,
                     SSVC_OPEN_CONNECT_UART_RX, UART_PIN_NO_CHANGE,
                     UART_PIN_NO_CHANGE);
  if (ret != ESP_OK) {
    ESP_LOGE("SsvcConnector", "uart_set_pin failed: %s", esp_err_to_name(ret));
    return;
  }
  gpio_set_pull_mode(SSVC_OPEN_CONNECT_UART_TX, GPIO_PULLUP_ONLY);
  ret = uart_driver_install(SSVC_OPEN_CONNECT_UART_NUM,
                            SSVC_OPEN_CONNECT_BUF_SIZE * 2, 0, 0, nullptr,
                            ESP_INTR_FLAG_LEVEL3);
  if (ret != ESP_OK) {
    ESP_LOGE("SsvcConnector", "uart_driver_install failed: %s",
             esp_err_to_name(ret));
    return;
  }
  ESP_LOGV("SsvcConnector", "Starting loop task");

  SsvcConnector::initSsvcController();
};

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

[[noreturn]] void SsvcConnector::_telemetry(void *pvParameters) {
  auto *self = static_cast<SsvcConnector *>(pvParameters);
  static char data[1024];        // Буфер для данных
  const int errorThreshold = 10; // Количество ошибок для признаков сбоя
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

    while (true) {
      // Чтение одного байта за раз
      int len = uart_read_bytes(SSVC_OPEN_CONNECT_UART_NUM,
                                (uint8_t *)&data[idx], 1, pdMS_TO_TICKS(100));
      if (len > 0) {
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
        vTaskDelay(pdMS_TO_TICKS(100));
      }
    }

    // После чтения всей строки пробуем десериализовать JSON
#ifdef CONFIG_IDF_TARGET_ESP32S3
    SpiRamAllocator allocator;
    JsonDocument doc(&allocator);
#else
    JsonDocument doc;
#endif
    DeserializationError error = deserializeJson(doc, data);
    if (error) {
      errorCounter++;
      ESP_LOGE("SsvcConnector", "Ошибка десериализации: %s", error.c_str());

      if (errorCounter >= errorThreshold) {
        self->uartCommunicationError = true;
      }
    } else {

      ESP_LOGI("SsvcConnector", "Начало вывода данных", data);
      ESP_LOGI("SsvcConnector", "%s", data);
      ESP_LOGI("SsvcConnector", "Конец вывода данных");

      if (xSemaphoreTake(mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        self->lastMessage = std::string(data);
        ESP_LOGV("SsvcConnector", "lastMessage: %s", self->lastMessage.c_str());
        xSemaphoreGive(mutex); // Копирование данных
      } else {
        ESP_LOGE("SsvcConnector",
                 "Не удалось захватить мьютекс для lastMessage");
      }

      self->uartCommunicationError = false;
      if (doc["type"] == "response") {
        if (doc["request"] == "GET_SETTINGS") {
          ESP_LOGV("SsvcConnector", "GET_SETTINGS: SEND BIT10");
          xEventGroupSetBits(eventGroup, BIT10);
        } else if (doc["result"] == "OK") {
          ESP_LOGV("SsvcConnector", "result: SEND BIT9 start");
          ESP_LOGV("SsvcConnector", "END BIT9 lastMessage: %s",
                   self->lastMessage.c_str());
          self->lastResponse = std::string(data);
          xEventGroupSetBits(eventGroup, BIT9);
          ESP_LOGV("SsvcConnector", "result: SEND BIT9 -end");
        } else {
          ESP_LOGE("SsvcConnector", "ERROR send command");
          xEventGroupSetBits(eventGroup, BIT1);
        }
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

    vTaskDelay(pdMS_TO_TICKS(500)); // Пауза для предотвращения перегрузки
  }
}

bool SsvcConnector::sendCommand(const char *command) {
  char *commandCopy = strdup(command);
  if (!commandCopy) {
    ESP_LOGE("SsvcConnector", "Ошибка копирования команды");
    return false;
  }
  ESP_LOGI("SsvcConnector", "Отправка команды SSVC: %s", commandCopy);
  ESP_LOGI("SsvcConnector", "strlen: %d", strlen(commandCopy));
  ssize_t bytes_written = uart_write_bytes(
      UART_NUM_1, (const char *)commandCopy, strlen(commandCopy));
  bool result = (bytes_written == strlen(commandCopy));
  ESP_LOGI("sendCommand", "result: %s", result ? "true" : "false");
  uart_wait_tx_done(UART_NUM_1, pdMS_TO_TICKS(1000));
  free(commandCopy); // Освобождаем память
  return result;
}

std::string SsvcConnector::getLastMessage() { return lastMessage; }

std::string SsvcConnector::getLastResponse() { return lastResponse; }
