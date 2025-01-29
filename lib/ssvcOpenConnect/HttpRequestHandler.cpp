//
// Created by demoncat on 03.12.2024.
//

#include "HttpRequestHandler.h"


HttpRequestHandler::HttpRequestHandler(PsychicHttpServer& server,
                                       SecurityManager* securityManager,
                                       RectificationProcess& rProcess,
                                       SsvcConnector& ssvcConnector,
                                       SsvcSettings& ssvcSettings) : _server(server),
                                                                     _securityManager(securityManager),
                                                                     _rProcess(rProcess),
                                                                     _ssvcConnector(ssvcConnector),
                                                                     _ssvcSettings(ssvcSettings)

{
}

void HttpRequestHandler::begin()
{
    Serial.println("Init HttpRequestHandler");

    _server.on(GET_STATUS_ROUTE,
               HTTP_GET,
               _securityManager->wrapRequest(std::bind(&HttpRequestHandler::getStatus, this, std::placeholders::_1),
                                             AuthenticationPredicates::IS_AUTHENTICATED));

    _server.on(GET_SETTINGS_ROUTE,
                HTTP_GET,
                _securityManager->wrapRequest(std::bind(&HttpRequestHandler::getSettings, this, std::placeholders::_1),
                                              AuthenticationPredicates::IS_AUTHENTICATED));


    _server.on(SEND_COMMAND_ROUTE,
                HTTP_POST,
                _securityManager->wrapRequest(
                        std::bind(&HttpRequestHandler::postCommandStatusStatus, this, std::placeholders::_1),
                        AuthenticationPredicates::IS_AUTHENTICATED));

    _server.on(TEMP_METRICS_DATA_ROUTE,
                HTTP_GET,
                _securityManager->wrapRequest(std::bind(&HttpRequestHandler::tMetrixResponce, this, std::placeholders::_1),
                                              AuthenticationPredicates::IS_AUTHENTICATED));
}

esp_err_t HttpRequestHandler::getStatus(PsychicRequest *request)
{

    PsychicJsonResponse response = PsychicJsonResponse(request, false);
    JsonObject root = response.getRoot();
    //work with some params
    if (request->hasParam("type")){
        std::string type = request->getParam("type")->value().c_str();
        root["request"] = type;
        JsonVariant _response =  root["response"].to<JsonVariant>();
        if (type == "ssvc") {
            if (_rProcess.getStatus(_response)) {
                response.setCode(200);
            } else {
                response.setCode(500);
            }
        } else {
            response.setCode(501);
        }
    } else {
        response.setCode(501);
    }

    return response.send();
}

esp_err_t HttpRequestHandler::getSettings(PsychicRequest *request)
{

    PsychicJsonResponse response = PsychicJsonResponse(request, false);
    JsonObject root = response.getRoot();
    root["request"] = "settings";
    JsonVariant _response =  root["settings"].to<JsonVariant>();
    _ssvcSettings.getSettings(_response);
    response.setCode(200);

    return response.send();
}

esp_err_t HttpRequestHandler::postCommandStatusStatus(PsychicRequest *request)
{
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
    if ( commandName == "next" )  {
        _ssvcConnector.sendNextCommand();
    } else if ( commandName == "pause" ) {
        _ssvcConnector.sendPauseCommand();
    } else if ( commandName == "stop" ) {
        _ssvcConnector.sendStopCommand();
    }else if ( commandName == "resume" ) {
        _ssvcConnector.sendResumeCommand();
    }else if (commandName == "version") {
        _ssvcConnector.sendVersionCommand();
    } else if (commandName == "settings") {
        _ssvcConnector.sendGetSettingsCommand();
    } else if (commandName == "set") {
        std::string settings = json["setting"].as<std::string>();
        size_t length = settings.size();
        if (length == 0) {
            root["error"] = "не верная длинна настроек ssvc";
            return request->reply(501);
        } else {
            _ssvcConnector.sendSetCommand(settings);
        }
    }
    else {
        return request->reply(501);
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
//    JsonDocument doc = _rProcess.getGraphTempData(point, periodicity);
    JsonDocument doc;

    // Преобразуем JsonDocument в JsonObject и добавляем его в корневой объект
    root["graphData"] = doc;  // Добавляем данные в корневой объект

    // Отправляем ответ
    return response.send();
}
