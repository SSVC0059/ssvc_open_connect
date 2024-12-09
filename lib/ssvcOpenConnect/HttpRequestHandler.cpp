//
// Created by demoncat on 03.12.2024.
//

#include "HttpRequestHandler.h"


HttpRequestHandler::HttpRequestHandler(PsychicHttpServer *server,
                                       SecurityManager* securityManager,
                                       RectificationProcess* rProcess,
                                       SsvcConnector* ssvcConnector) : _server(server),
                                                                         _securityManager(securityManager),
                                                                         _rProcess(rProcess),
                                                                         _ssvcConnector(ssvcConnector)

{
}

void HttpRequestHandler::begin()
{
    Serial.println("Init HttpRequestHandler");
    _server->on(GET_SETTINGS_ROUTE,
                HTTP_GET,
                _securityManager->wrapRequest(std::bind(&HttpRequestHandler::reqStatus, this, std::placeholders::_1),
                                              AuthenticationPredicates::IS_AUTHENTICATED));

    ESP_LOGV("HttpRequestHandler", "Registered GET endpoint: %s", GET_SETTINGS_ROUTE);

    _server->on(SEND_COMMAND_ROUTE,
                HTTP_POST,
                _securityManager->wrapRequest(
                        std::bind(&HttpRequestHandler::postCommandStatusStatus, this, std::placeholders::_1),
                        AuthenticationPredicates::IS_AUTHENTICATED));

    _server->on(TEMP_METRICS_DATA_ROUTE,
                HTTP_GET,
                _securityManager->wrapRequest(std::bind(&HttpRequestHandler::tMetrixResponce, this, std::placeholders::_1),
                                              AuthenticationPredicates::IS_AUTHENTICATED));
}


esp_err_t HttpRequestHandler::reqStatus(PsychicRequest *request)
{
    PsychicJsonResponse response = PsychicJsonResponse(request, false);
    JsonObject root = response.getRoot();
    root["settings"] = _rProcess->getSsvcSettings()->as<JsonObject>();
    root["status"] =  "ok";

    return response.send();
}

esp_err_t HttpRequestHandler::postCommandStatusStatus(PsychicRequest *request)
{
    JsonDocument jsonBuffer;
    DeserializationError error = deserializeJson(jsonBuffer, request->body());
    if (error)
        return request->reply(400);

    JsonVariant json = jsonBuffer.as<JsonVariant>();

    String commandName = json["commands"];
    Serial.print("common name: ");
    Serial.println(commandName);
//        TODO переделать работу с командами
    if ( commandName == "next" )  {
        _ssvcConnector->taskNestCommand();
    } else if ( commandName == "pause" ) {
        _ssvcConnector->taskPauseCommand();
    } else if ( commandName == "stop" ) {
        _ssvcConnector->taskStopCommand();
    }else if ( commandName == "resume" ) {
        _ssvcConnector->taskResumeCommand();
    }else if (commandName == "version") {
        _ssvcConnector->taskVersionCommand();
    } else if (commandName == "settings") {
        _ssvcConnector->taskGetSettingsCommand();
    }else {
        request->reply(501);
    }

    return request->reply(200);
}

esp_err_t HttpRequestHandler::tMetrixResponce(PsychicRequest *request)
{
    PsychicJsonResponse response(request, false);  // Создается объект ответа на запрос

    int point = 0;
    int periodicity = 12;

    //work with some params
    if (request->hasParam("point"))
    {
        point = request->getParam("point")->value().toInt();
    }

    if (request->hasParam("periodicity"))
    {
        periodicity = request->getParam("periodicity")->value().toInt();
    }

    JsonObject root = response.getRoot();  // Получаем корневой объект из ответа

    Serial.print("point: ");
    Serial.print(point);
    Serial.print("periodicity: ");
    Serial.print(periodicity);

    // Получаем данные в виде JsonDocument
    JsonDocument doc = _rProcess->getGraphTempData(point, periodicity);

    // Преобразуем JsonDocument в JsonObject и добавляем его в корневой объект
    root["graphData"] = doc;  // Добавляем данные в корневой объект

    // Отправляем ответ
    return response.send();
}
