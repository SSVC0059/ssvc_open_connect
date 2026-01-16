/**
 * SSVC Open Connect
 *
 * A firmware for ESP32 to interface with SSVC 0059 distillation controller
 * via UART protocol. Features a responsive SvelteKit web interface for
 * monitoring and controlling the distillation process.
 * https://github.com/SSVC0059/ssvc_open_connect
 *
 * Copyright (C) 2024 SSVC Open Connect Contributors
 *
 * This software is independent and not affiliated with SSVC0059 company.
 * All Rights Reserved. This software may be modified and distributed under
 * the terms of the LGPL v3 license. See the LICENSE file for details.
 * * Disclaimer: Use at your own risk. High voltage safety precautions required.
 **/

#include "MqttCommandHandler.h"

MqttCommandHandler::MqttCommandHandler() = default;

void MqttCommandHandler::begin() const
{
    ESP_LOGI(TAG, "Регистрирую подписку на топик команд: %s", MQTT_CMD_TOPIC);

    MqttBridge &bridge = MqttBridge::getInstance();
    bridge.registerSubscription(MQTT_CMD_TOPIC, 1);
    bridge.setMessageHandler(
        [this](const String &topic, const String &payload, int qos, bool retained, bool duplicated) {
            this->onMqttCommandReceived(topic, payload);
        }
    );
}

void MqttCommandHandler::onMqttCommandReceived(const String& topic, const String& payload)
{
    ESP_LOGI(TAG, "Получена команда: %s в топик %s", payload.c_str(), topic.c_str());
    handleCommand(payload);
}

void MqttCommandHandler::handleCommand(const String& commandString)
{
    if (commandString.length() == 0) {
        ESP_LOGW(TAG, "Пустая строка команды.");
        return;
    }

    const std::string commandInput = commandString.c_str();

    // Преобразуем всю команду к нижнему регистру для регистронезависимого поиска имени команды
    std::string commandLowerCase = commandInput;
    std::transform(commandLowerCase.begin(), commandLowerCase.end(), commandLowerCase.begin(),
                   [](const unsigned char c){ return std::tolower(c); });

    const size_t spacePos = commandLowerCase.find(' ');
    const std::string commandName = (spacePos == std::string::npos) ? commandLowerCase : commandLowerCase.substr(0, spacePos);

    const auto it = SsvcCommandsQueue::COMMAND_MAP.find(commandName);

    if (it != SsvcCommandsQueue::COMMAND_MAP.end()) {
        std::string params;
        if (spacePos != std::string::npos) {
            // Извлекаем параметры из оригинальной строки, чтобы сохранить регистр
            params = commandInput.substr(spacePos + 1);
        }

        ESP_LOGI(TAG, "-> Найдена команда '%s'. Вызываем с параметрами: '%s'", commandName.c_str(), params.c_str());

        // Вызываем лямбда-функцию из COMMAND_MAP, передавая параметры
        it->second(params);

        ESP_LOGI(TAG, "<- Команда '%s' поставлена в очередь.", commandName.c_str());
    } else {
        // Неизвестная команда
        ESP_LOGW(TAG, "Неизвестная команда: %s", commandName.c_str());
        const std::string errorMessage = commandName + " - неизвестная команда.";

        // Отправляем ответ об ошибке через MqttBridge
        (void)MqttBridge::getInstance().publish(MQTT_RSP_TOPIC, errorMessage.c_str(), 1, false);
    }
}