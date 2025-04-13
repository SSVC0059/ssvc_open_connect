//
// Created by demoncat on 23.12.2024.
//

#ifndef SSVC_OPEN_CONNECT_SSVCOPENCONNECT_H
#define SSVC_OPEN_CONNECT_SSVCOPENCONNECT_H

#include "utility"
#include "API/HttpRequestHandler.h"
#include "ESP32SvelteKit.h"
#include "EventSocket.h"
#include "HttpEndpoint.h"
#include "OpenConnectSettings.h"
#include "SecurityManager.h"
#include "SsvcCommandsQueue.h"
#include "SsvcConnector.h"
#include "SsvcSettings.h"
#include "rectification/RectificationProcess.h"

/**
 * @def TASK_AT_COMMAND_SEND_STACK_PERIOD
 * @brief Период отправки AT-команд в секундах
 */
#define TASK_AT_COMMAND_SEND_STACK_PERIOD 60

#if __cplusplus < 201402L
namespace std {
/**
 * @brief Реализация make_unique для C++11
 */
template <typename T, typename... Args>
unique_ptr<T> make_unique(Args &&...args) {
  return unique_ptr<T>(new T(std::forward<Args>(args)...));
}
} // namespace std
#endif

/**
 * @class SsvcOpenConnect
 * @brief Основной класс для работы с SSVC устройством через WiFi
 *
 * Класс реализует:
 * - Управление подключением к SSVC устройству
 * - Обработку команд устройства
 * - Взаимодействие через HTTP API
 */
class SsvcOpenConnect {
public:
  /**
   * @brief Получить экземпляр класса (Singleton)
   * @param server HTTP сервер
   * @param esp32sveltekit Фреймворк ESP32SvelteKit
   * @param socket Сокет для событий
   * @param securityManager Менеджер безопасности
   * @return Указатель на экземпляр класса
   */
  static SsvcOpenConnect *getInstance(PsychicHttpServer &server,
                                      ESP32SvelteKit &esp32sveltekit,
                                      EventSocket *socket,
                                      SecurityManager *securityManager) {
    if (!instance) {
      instance =
          new SsvcOpenConnect(server, esp32sveltekit, socket, securityManager);
    }
    return instance;
  }

  /**
   * @brief Инициализация модуля
   */
  void begin();

private:
  /**
   * @brief Конструктор
   */
  SsvcOpenConnect(PsychicHttpServer &server, ESP32SvelteKit &esp32sveltekit,
                  EventSocket *socket, SecurityManager *securityManager);

  PsychicHttpServer &_server;        ///< HTTP сервер
  ESP32SvelteKit &_esp32sveltekit;   ///< Фреймворк ESP32SvelteKit
  EventSocket *_socket;              ///< Сокет для событий
  SecurityManager *_securityManager; ///< Менеджер безопасности

  SsvcConnector &_ssvcConnector; ///< Коннектор к SSVC устройству
  OpenConnectSettingsService _openConnectSettingsService; ///< Сервис настроек
  SsvcSettings &_ssvcSettings;    ///< Настройки SSVC устройства
  RectificationProcess &rProcess; ///< Процесс ректификации
  std::unique_ptr<HttpRequestHandler>
      httpRequestHandler; ///< HTTP обработчик запросов

  static SsvcOpenConnect
      *instance; ///< Единственный экземпляр класса (Singleton)
};

#endif // SSVC_OPEN_CONNECT_SSVCOPENCONNECT_H