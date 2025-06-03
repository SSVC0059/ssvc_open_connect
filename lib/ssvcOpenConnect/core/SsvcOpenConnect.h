//
// Created by demoncat on 23.12.2024.
//

#ifndef SSVC_OPEN_CONNECT_SSVCOPENCONNECT_H
#define SSVC_OPEN_CONNECT_SSVCOPENCONNECT_H

#include "utility"
#include "API/HttpRequestHandler.h"
#include "ESP32SvelteKit.h"
#include "EventSocket.h"

#include "SecurityManager.h"
#include "SsvcConnector.h"
#include "SsvcSettings/SsvcSettings.h"
#include <rectification/RectificationProcess.h>
#include <sensors/Thermal/ThermalSensors.h>

/**
 * @def TASK_AT_COMMAND_SEND_STACK_PERIOD
 * @brief Период отправки AT-команд в секундах
 */
#define TASK_AT_COMMAND_SEND_STACK_PERIOD 60


/**
 * @class SsvcOpenConnect
 * @brief Основной класс для работы с SSVC устройством через WiFi
 */
class SsvcOpenConnect
{
public:
  /**
   * @brief Получить экземпляр класса (Singleton)
   */
  static SsvcOpenConnect& getInstance() {
    static SsvcOpenConnect instance;
    return instance;
  }

  static void subsystemManager();

  /**
   * @brief Инициализация модуля
   */
  void begin(PsychicHttpServer& server,
             ESP32SvelteKit& esp32sveltekit,
             EventSocket* socket,
             SecurityManager* securityManager);

  // Удаляем конструкторы копирования и присваивания
  SsvcOpenConnect(const SsvcOpenConnect&) = delete;
  void operator=(const SsvcOpenConnect&) = delete;

  bool isOnline() const;
private:
  /**
   * @brief Конструктор (приватный для Singleton)
   */
  SsvcOpenConnect() = default;

  PsychicHttpServer* _server = nullptr;       ///< HTTP сервер
  ESP32SvelteKit* _esp32sveltekit = nullptr; ///< Фреймворк ESP32SvelteKit
  EventSocket* _socket = nullptr;            ///< Сокет для событий
  SecurityManager* _securityManager = nullptr; ///< Менеджер безопасности

  SsvcConnector& _ssvcConnector = SsvcConnector::getConnector(); ///< Коннектор к SSVC
  OpenConnectSettingsService* _openConnectSettingsService = nullptr; ///< Сервис настроек
  SsvcSettings& _ssvcSettings = SsvcSettings::init(); ///< Настройки SSVC
  RectificationProcess& rProcess = RectificationProcess::rectController();
  ThermalSensors& _thermalSensors = ThermalSensors::thermalController();

  std::unique_ptr<HttpRequestHandler> httpRequestHandler; ///< HTTP обработчик


  static constexpr auto TAG = "SsvcOpenConnect";

};

#endif // SSVC_OPEN_CONNECT_SSVCOPENCONNECT_H