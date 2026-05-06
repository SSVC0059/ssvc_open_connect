#include "core/SsvcOpenConnect.h"
#include <esp_log.h>
#include <ESP32SvelteKit.h>
#include <ESPAsyncWebServer.h>
#include <esp_task_wdt.h>
#include <components/Led/StatusLed.h>

#define SERIAL_BAUD_RATE 115200

// #define configCHECK_FOR_STACK_OVERFLOW 2

portMUX_TYPE ssvcMux = portMUX_INITIALIZER_UNLOCKED;
SemaphoreHandle_t mutex = xSemaphoreCreateMutex();
EventGroupHandle_t eventGroup = xEventGroupCreate();

AsyncWebServer server(80);

ESP32SvelteKit esp32sveltekit(&server);

void setup() {
  // Инициализация последовательного порта
  Serial.begin(SERIAL_BAUD_RATE);

  // Вывод информации о памяти
  Serial.printf("Total heap: %d\n", ESP.getHeapSize());
  Serial.printf("Free heap: %d\n", ESP.getFreeHeap());
  Serial.printf("Min free heap: %d\n", ESP.getMinFreeHeap());

  // Настройка mDNS имени, если определено
#ifdef FACTORY_WIFI_HOSTNAME
  esp32sveltekit.setMDNSAppName(FACTORY_WIFI_HOSTNAME);
#endif

  // Запуск ESP32-SvelteKit
  esp32sveltekit.begin();

  ESP_LOGI("main", "setup: starting SsvcOpenConnect::begin()");
  // Получение экземпляра SsvcOpenConnect и его инициализация
  SsvcOpenConnect::getInstance().begin(
      server,
      esp32sveltekit,
      esp32sveltekit.getSocket(),
      esp32sveltekit.getSecurityManager()
  );
  ESP_LOGI("main", "setup: SsvcOpenConnect::begin() returned");
}

void loop() {
  vTaskDelete(nullptr);
}

extern "C" void vApplicationStackOverflowHook(TaskHandle_t xTask,
                                              char *pcTaskName) {
  ESP_LOGE("FreeRTOS", "Stack overflow in task %s", pcTaskName);
  abort(); // Остановка работы для диагностики
}