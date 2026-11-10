#include "core/SsvcOpenConnect.h"
#include <esp_log.h>
#include <ESP32SvelteKit.h>
#include <ESPAsyncWebServer.h>
#include <esp_task_wdt.h>
#include <components/Led/StatusLed.h>
#include <esp_ota_ops.h>
#include <esp_system.h>

#define SERIAL_BAUD_RATE 115200

// -----------------------------------------------------------------------------
// Build identity banner.
//
// Printed once at startup so a serial log can be unambiguously matched with the
// debug ELF. The sha256 below is the hash of firmware.elf stored by the build
// script, and the same hash is used as the file name:
//     build/elf/<target>/<elf_sha256>.elf
// so `grep "\[BUILD\]"` is enough to pick the right ELF for addr2line.
// -----------------------------------------------------------------------------
static const char *resetReasonName(esp_reset_reason_t reason) {
  switch (reason) {
    case ESP_RST_POWERON:   return "POWERON";
    case ESP_RST_EXT:       return "EXT";
    case ESP_RST_SW:        return "SW";
    case ESP_RST_PANIC:     return "PANIC";
    case ESP_RST_INT_WDT:   return "INT_WDT";
    case ESP_RST_TASK_WDT:  return "TASK_WDT";
    case ESP_RST_WDT:       return "WDT";
    case ESP_RST_DEEPSLEEP: return "DEEPSLEEP";
    case ESP_RST_BROWNOUT:  return "BROWNOUT";
    case ESP_RST_SDIO:      return "SDIO";
    default:                return "UNKNOWN";
  }
}

static void logBuildIdentity() {
  char elfSha[65] = {0};
  const esp_err_t shaErr = esp_ota_get_app_elf_sha256(elfSha, sizeof(elfSha));
  if (shaErr != ESP_OK) {
    snprintf(elfSha, sizeof(elfSha), "unavailable_0x%X", static_cast<unsigned>(shaErr));
  }

  // IDF 4.4 (Arduino core 3.2): описание приложения берётся из esp_ota_ops.h.
  const esp_app_desc_t *desc = esp_ota_get_app_description();

  ESP_LOGI("BUILD", "===== BUILD IDENTITY =====");
  ESP_LOGI("BUILD", "app=%s version=%s target=%s", APP_NAME, APP_VERSION, BUILD_TARGET);
  ESP_LOGI("BUILD", "compiled=%s %s  image=%s %s", __DATE__, __TIME__,
           desc->date, desc->time);
  ESP_LOGI("BUILD", "idf=%s", desc->idf_ver);
  ESP_LOGI("BUILD", "elf_sha256=%s", elfSha);
  ESP_LOGI("BUILD", "elf_lookup=build/elf/%s/%s.elf", BUILD_TARGET, elfSha);
  ESP_LOGI("BUILD", "chip=%s rev=%u cores=%u flash=%u sketch=%u",
           ESP.getChipModel(), static_cast<unsigned>(ESP.getChipRevision()),
           static_cast<unsigned>(ESP.getChipCores()),
           static_cast<unsigned>(ESP.getFlashChipSize()),
           static_cast<unsigned>(ESP.getSketchSize()));
  ESP_LOGI("BUILD", "reset=%s", resetReasonName(esp_reset_reason()));
}

// #define configCHECK_FOR_STACK_OVERFLOW 2

portMUX_TYPE ssvcMux = portMUX_INITIALIZER_UNLOCKED;
SemaphoreHandle_t mutex = xSemaphoreCreateMutex();
EventGroupHandle_t eventGroup = xEventGroupCreate();

AsyncWebServer server(80);

ESP32SvelteKit esp32sveltekit(&server);

void setup() {
  // Инициализация последовательного порта
  Serial.begin(SERIAL_BAUD_RATE);

  // Сопоставление лога с отладочным ELF: версия, env, sha256 ELF
  logBuildIdentity();

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