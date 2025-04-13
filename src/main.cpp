 #include "components/StatusLed.h"
 #include "core/SsvcOpenConnect.h"
 #include <ESP32SvelteKit.h>
 #include <PsychicHttpServer.h>
 
 #define SERIAL_BAUD_RATE 115200
 
 // #define configCHECK_FOR_STACK_OVERFLOW 2
 
 portMUX_TYPE ssvcMux = portMUX_INITIALIZER_UNLOCKED;
 SemaphoreHandle_t mutex = xSemaphoreCreateMutex();
 EventGroupHandle_t eventGroup = xEventGroupCreate();
 
 PsychicHttpServer server;
 
 ESP32SvelteKit esp32sveltekit(&server, 120);
 StatusLed statusLed(&esp32sveltekit);
 
 SsvcOpenConnect *SsvcOpenConnect::instance = nullptr;
 
 void setup() {
   SsvcOpenConnect *instance = SsvcOpenConnect::getInstance(
       server, esp32sveltekit, esp32sveltekit.getSocket(),
       esp32sveltekit.getSecurityManager());
 
   // start serial and filesystem
   Serial.begin(SERIAL_BAUD_RATE);
 
 #ifdef FACTORY_WIFI_HOSTNAME
   esp32sveltekit.setMDNSAppName(FACTORY_WIFI_HOSTNAME);
 #endif
   // start ESP32-SvelteKit
   esp32sveltekit.begin();
 #if CONFIG_IDF_TARGET_ESP32S3
   statusLed.begin();
 #endif
 
   instance->begin();
 }
 
 void loop() { vTaskDelete(nullptr); }
 
 extern "C" void vApplicationStackOverflowHook(TaskHandle_t xTask,
                                               char *pcTaskName) {
   ESP_LOGE("FreeRTOS", "Stack overflow in task %s", pcTaskName);
   abort(); // Остановка работы для диагностики
 }
 