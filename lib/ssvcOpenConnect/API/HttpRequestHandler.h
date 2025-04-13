//
// Created by demoncat on 03.12.2024.
//

#ifndef SSVC_OPEN_CONNECT_HTTPREQUESTHANDLER_H
#define SSVC_OPEN_CONNECT_HTTPREQUESTHANDLER_H

#include "ArduinoJson.h"
#include "PsychicHttp.h"
#include "SecurityManager.h"
#include "SettingsHandlers.h"
#include "core/SsvcCommandsQueue.h"
#include "rectification/RectificationProcess.h"
#include <Arduino.h>
#include <esp_http_server.h>

#define GET_TELEMETRY_ROUTE "/rest/telemetry"
#define GET_SETTINGS_ROUTE "/rest/settings"
#define SEND_COMMAND_ROUTE "/rest/commands"
#define TEMP_METRICS_DATA_ROUTE "/rest/metrics"

#include <atomic>

class HttpRequestHandler {
public:
  HttpRequestHandler(PsychicHttpServer &server,
                     SecurityManager *securityManager,
                     RectificationProcess &rProcess);

  void begin();

private:
  PsychicHttpServer &_server;
  SecurityManager *_securityManager;
  RectificationProcess &_rProcess;

  esp_err_t telemetry(PsychicRequest *request);

  esp_err_t getSsvcSettings(PsychicRequest *request);

  esp_err_t UpdateSsvcSettings(PsychicRequest *request);

  esp_err_t postCommandStatusStatus(PsychicRequest *request);

  esp_err_t tMetrixResponse(PsychicRequest *request);

  void parseQueryParams(const String &query,
                        std::vector<std::pair<String, String>> &output) {
    unsigned int start = 0;
    while (start < query.length()) {
      // Ищем конец пары
      unsigned int end = query.indexOf('&', start);
      if (end == -1)
        end = query.length();

      // Вырезаем отдельную пару
      String pair = query.substring(start, end);

      // Разделяем на ключ и значение
      int eqPos = pair.indexOf('=');
      String name = (eqPos != -1) ? pair.substring(0, eqPos) : pair;
      String value = (eqPos != -1) ? pair.substring(eqPos + 1) : "";

      // Добавляем в результат
      output.emplace_back(name, value);

      // Переходим к следующей паре
      start = end + 1;
    }
  }
};

#endif // SSVC_OPEN_CONNECT_HTTPREQUESTHANDLER_H
