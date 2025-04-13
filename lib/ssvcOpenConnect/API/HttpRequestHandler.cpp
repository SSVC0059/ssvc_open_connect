//
// Created by demoncat on 03.12.2024.
//

#include "HttpRequestHandler.h"
#include "core/SsvcOpenConnect.h"

HttpRequestHandler::HttpRequestHandler(PsychicHttpServer &server,
                                       SecurityManager *securityManager,
                                       RectificationProcess &rProcess)
    : _server(server), _securityManager(securityManager), _rProcess(rProcess) {}

void HttpRequestHandler::begin() {
  Serial.println("Init HttpRequestHandler");

  _server.on(GET_TELEMETRY_ROUTE, HTTP_GET,
             _securityManager->wrapRequest(
                 std::bind(&HttpRequestHandler::telemetry, this,
                           std::placeholders::_1),
                 AuthenticationPredicates::IS_AUTHENTICATED));

  _server.on(GET_SETTINGS_ROUTE, HTTP_GET,
             _securityManager->wrapRequest(
                 std::bind(&HttpRequestHandler::getSsvcSettings, this,
                           std::placeholders::_1),
                 AuthenticationPredicates::IS_AUTHENTICATED));

  _server.on(GET_SETTINGS_ROUTE, HTTP_PUT,
             _securityManager->wrapRequest(
                 std::bind(&HttpRequestHandler::UpdateSsvcSettings, this,
                           std::placeholders::_1),
                 AuthenticationPredicates::IS_AUTHENTICATED));

  _server.on(SEND_COMMAND_ROUTE, HTTP_POST,
             _securityManager->wrapRequest(
                 std::bind(&HttpRequestHandler::postCommandStatusStatus, this,
                           std::placeholders::_1),
                 AuthenticationPredicates::IS_AUTHENTICATED));

  _server.on(TEMP_METRICS_DATA_ROUTE, HTTP_GET,
             _securityManager->wrapRequest(
                 std::bind(&HttpRequestHandler::tMetrixResponse, this,
                           std::placeholders::_1),
                 AuthenticationPredicates::IS_AUTHENTICATED));
}

esp_err_t HttpRequestHandler::telemetry(PsychicRequest *request) {
  PsychicJsonResponse response = PsychicJsonResponse(request, false);
  JsonObject root = response.getRoot();
  std::string message = _rProcess.getTelemetry();
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, message);
  if (error) {
    Serial.print("Failed to parse JSON: ");
    Serial.println(error.c_str());
    response.setCode(500);
    response.send();
  }

#if CONFIG_LOG_DEFAULT_LEVEL >=                                                \
    ESP_LOG_DEBUG // Выполняем только при уровне Debug или выше
  // Создаем строку для хранения сериализованного JSON
  // Логируем JSON
  ESP_LOGV("RECTIFICATION", "send: %s", message.c_str());
#endif
  root["telemetry"] = doc;

  if (request->hasParam("mode")) {
    std::string type = request->getParam("mode")->value().c_str();
    root["request"] = type;
    JsonVariant _response = root["status"].to<JsonVariant>();
    if (type == "status") {
      if (_rProcess.getStatus(_response)) {
        response.setCode(200);
      } else {
        response.setCode(500);
      }
    }
  }

  return response.send();
}

esp_err_t HttpRequestHandler::getSsvcSettings(PsychicRequest *request) {

  PsychicJsonResponse response = PsychicJsonResponse(request, false);
  JsonObject root = response.getRoot();
  root["request"] = "settings";
  JsonVariant _response = root["settings"].to<JsonVariant>();
  SsvcSettings::init().getSettings(_response);
  response.setCode(200);

  return response.send();
}

esp_err_t HttpRequestHandler::postCommandStatusStatus(PsychicRequest *request) {
  JsonDocument jsonBuffer;
  DeserializationError error = deserializeJson(jsonBuffer, request->body());
  PsychicJsonResponse response = PsychicJsonResponse(request, false);
  JsonObject root = response.getRoot();
  if (error) {
    root["error"] = "ошибка десериализации";
    return request->reply(400);
  }

  JsonVariant json = jsonBuffer.as<JsonVariant>();
  std::string commandName = json["commands"].as<std::string>();
  if (commandName == "next") {
    SsvcCommandsQueue::getQueue().next();
  } else if (commandName == "pause") {
    SsvcCommandsQueue::getQueue().pause();
  } else if (commandName == "stop") {
    SsvcCommandsQueue::getQueue().stop();
  } else if (commandName == "start") {
    SsvcCommandsQueue::getQueue().start();
  } else if (commandName == "resume") {
    SsvcCommandsQueue::getQueue().resume();
  } else if (commandName == "version") {
    SsvcCommandsQueue::getQueue().version();
  } else if (commandName == "settings") {
    SsvcCommandsQueue::getQueue().getSettings();
  } else {
    return request->reply(501);
  }

  return request->reply(200);
}

esp_err_t HttpRequestHandler::tMetrixResponse(PsychicRequest *request) {
  PsychicJsonResponse response(request,
                               false); // Создается объект ответа на запрос

  int point = 0;
  int periodicity = 12;

  // work with some params
  if (request->hasParam("point")) {
    point = request->getParam("point")->value().toInt();
  }

  if (request->hasParam("periodicity")) {
    periodicity = request->getParam("periodicity")->value().toInt();
  }

  JsonObject root = response.getRoot(); // Получаем корневой объект из ответа

  Serial.print("point: ");
  Serial.print(point);
  Serial.print("periodicity: ");
  Serial.print(periodicity);

  // Получаем данные в виде JsonDocument
  //    JsonDocument doc = _rProcess.getGraphTempData(point, periodicity);
  JsonDocument doc;

  // Преобразуем JsonDocument в JsonObject и добавляем его в корневой объект
  root["graphData"] = doc; // Добавляем данные в корневой объект

  // Отправляем ответ
  return response.send();
}

esp_err_t HttpRequestHandler::UpdateSsvcSettings(PsychicRequest *request) {

  String query = request->query();
  if (query.isEmpty()) {
    ESP_LOGV("", "No parameters received");
    return request->reply(400, "text/plain", "No parameters received");
  }

  JsonDocument jsonBuffer;
  std::vector<std::pair<String, String>> params;
  parseQueryParams(query, params);

  bool hasErrors = false;
  JsonObject errors = jsonBuffer.to<JsonObject>();
  SsvcSettings::Builder builder;

  for (const auto &param : params) { // Убрано декомпозирование
    const String &key = param.first;
    const String &valueStr = param.second;

    ESP_LOGV("HTTP", "Processing parameter: %s=%s", key.c_str(),
             valueStr.c_str());

    // Парсим значение как JSON
    JsonDocument valueDoc;
    DeserializationError err = deserializeJson(valueDoc, valueStr);
    if (err) {
      errors[key] = "Invalid JSON format";
      hasErrors = true;
      continue;
    }

    auto it = PARAM_HANDLERS.find(key);
    if (it == PARAM_HANDLERS.end()) {
      errors[key] = "Unknown parameter";
      hasErrors = true;
      continue;
    }

    if (!it->second->handle(builder, valueDoc.as<JsonVariant>())) {
      errors[key] = "Invalid value type";
      hasErrors = true;
    }
  }

  if (hasErrors) {
    String errorMsg;
    serializeJson(errors, errorMsg);
    return request->reply(400, "text/plain", errorMsg.c_str());
  }

  // Ожидание с проверкой флага

  ESP_LOGV("HTTP", "Waiting for _cmdSetResult...");
  TickType_t startTicks = xTaskGetTickCount();
  TickType_t timeoutTicks = pdMS_TO_TICKS(10000);
  boolean cmdResult = SsvcCommandsQueue::getQueue()._cmdSetResult;
  while (!cmdResult && (xTaskGetTickCount() - startTicks) < timeoutTicks) {
    ESP_LOGV("HTTP", "Waiting for _cmdSetResult...");
    vTaskDelay(pdMS_TO_TICKS(100));
  }

  ESP_LOGV("HTTP", "Finished waiting for _cmdSetResult.");

  if (!cmdResult) {
    return request->reply(408, "text/plain", "Timeout");
  }

  return cmdResult ? request->reply(200, "text/plain", "OK")
                   : request->reply(500, "text/plain", "Error");
}
