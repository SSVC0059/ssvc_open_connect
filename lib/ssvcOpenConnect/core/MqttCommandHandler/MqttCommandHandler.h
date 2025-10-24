//
// Created by Admin on 24/10/2025.
//

#ifndef SSVC_OPEN_CONNECT_MQTTCOMMANDHANDLER_H
#define SSVC_OPEN_CONNECT_MQTTCOMMANDHANDLER_H

/**
 *   SSVC Open Connect
 *
 *   A firmware for ESP32 to interface with SSVC 0059 distillation controller
 *   via UART protocol. Features a responsive SvelteKit web interface for
 *   monitoring and controlling the distillation process.
 *   https://github.com/SSVC0059/ssvc_open_connect
 *
 *   Copyright (C) 2024 SSVC Open Connect Contributors
 *
 *   This software is independent and not affiliated with SSVC0059 company.
 *   All Rights Reserved. This software may be modified and distributed under
 *   the terms of the LGPL v3 license. See the LICENSE file for details.
 *   
 *   Disclaimer: Use at your own risk. High voltage safety precautions required.
 **/

#include <string>
#include <map>
#include <functional>
#include <ArduinoJson.h>

#include <MqttSettingsService.h>
#include <PsychicMqttClient.h>
#include "core/SsvcCommandsQueue.h"

#include "external/MqttBridge/MqttBridge.h"


// Определения топиков для обмена командами/ответами
#define MQTT_CMD_TOPIC "openconnect/command/send"   // Топик для подписки и приема команд
#ifndef MQTT_RSP_TOPIC
#define MQTT_RSP_TOPIC "openconnect/command/response"
#endif


class MqttCommandHandler {
public:

    MqttCommandHandler();

    void begin() const;



private:
    /**
         * @brief Обработчик входящих MQTT сообщений, совместимый с MqttBridge.
         */
    static void onMqttCommandReceived(const String& topic, const String& payload);
    static void handleCommand(const String& commandString);

    static constexpr auto TAG = "MqttCmdHndl";
};

#endif //SSVC_OPEN_CONNECT_MQTTCOMMANDHANDLER_H