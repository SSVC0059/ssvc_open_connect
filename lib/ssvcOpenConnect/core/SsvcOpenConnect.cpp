//
// Created by demoncat on 23.12.2024.
//

#include "SsvcOpenConnect.h"

#include <utility>

/**
 * @brief Конструктор класса SsvcOpenConnect.
 *
 * @param server Ссылка на HTTP сервер
 * @param esp32sveltekit Ссылка на фреймворк ESP32SvelteKit
 * @param socket Указатель на сокет для событий
 * @param securityManager Указатель на менеджер безопасности
 *
 * Инициализирует все компоненты системы:
 * - Настройки подключения SSVC
 * - Сервис настроек OpenConnect
 * - Процесс ректификации
 * - HTTP обработчик запросов
 * - Очередь команд
 */
SsvcOpenConnect::SsvcOpenConnect(PsychicHttpServer &server,
                                 ESP32SvelteKit &esp32sveltekit,
                                 EventSocket *socket,
                                 SecurityManager *securityManager)
    : _server(server), _esp32sveltekit(esp32sveltekit), _socket(socket),
      _securityManager(securityManager),
      _ssvcConnector(SsvcConnector::getConnector()),
      _openConnectSettingsService(&server, &esp32sveltekit, securityManager),
      _ssvcSettings(SsvcSettings::init()),
      rProcess(RectificationProcess::createRectification(
          _ssvcConnector, _ssvcSettings, _openConnectSettingsService)) {
  httpRequestHandler =
      std::make_unique<HttpRequestHandler>(server, _securityManager, rProcess);
}

/**
 * @brief Инициализация модуля SsvcOpenConnect.
 *
 * Запускает:
 * 1. HTTP обработчик запросов
 * 2. Сервис настроек OpenConnect
 * 3. Устанавливает очередь команд для SSVC коннектора
 * 4. Запускает фоновую задачу обработки команд
 * 5. Регистрирует callback-обработчики команд
 * 6. Отправляет начальную команду GET_SETTINGS
 */
void SsvcOpenConnect::begin() {
  ESP_LOGI("SsvcOpenConnect", "Start SsvcOpenConnect begin()");
  _openConnectSettingsService.begin();
  httpRequestHandler->begin();
  vTaskDelay(pdMS_TO_TICKS(2000));
  SsvcCommandsQueue::getQueue().getSettings();
  SsvcCommandsQueue::getQueue().version();
}
