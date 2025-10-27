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

    // 1. Преобразуем входящую String в std::string для удобной работы
    std::string command = commandString.c_str();

    // 2. Преобразуем всю команду к нижнему регистру для регистронезависимого сравнения
    std::transform(command.begin(), command.end(), command.begin(),
                   [](unsigned char c){ return std::tolower(c); });

    // 3. Находим позицию первого пробела, чтобы отделить имя команды от параметров
    const size_t spacePos = command.find(' ');

    // 4. Извлекаем имя команды (которое теперь в нижнем регистре)
    const std::string commandName = (spacePos == std::string::npos)
                                    ? command
                                    : command.substr(0, spacePos);

    // 5. Проверяем команду SET
    if (commandName == "set") {

        if (spacePos == std::string::npos) {
            // Ошибка: команда SET без параметров
            ESP_LOGW(TAG, "Команда SET получена без параметров.");
            const auto payload = "SET - требует параметров.";
            const uint16_t msgId = MqttBridge::getInstance().publish(
                        MQTT_RSP_TOPIC,
                            payload,
                            1,
                            true);
            if (msgId == 0)
            {
                ESP_LOGE(TAG, "Error publishing status message");
            } else
            {
                ESP_LOGI(TAG, "Published status message: %s (Message ID: %d)", MQTT_RSP_TOPIC, msgId);
            }
            return;
        }

        // ВАЖНО: Параметры должны быть извлечены из ОРИГИНАЛЬНОЙ строки,
        // чтобы сохранить регистр и другие символы, если это необходимо для значений!
        const std::string parameters = commandString.c_str();
        const std::string parameters_payload = parameters.substr(spacePos + 1);

        ESP_LOGI(TAG, "-> [PARAM] Найдена команда 'SET'. Вызываем метод set() с параметрами: '%s'", parameters_payload.c_str());

        // Выполняем команду SET с параметрами
        SsvcCommandsQueue::getQueue().set(parameters_payload);

        ESP_LOGI(TAG, "<- [PARAM] Команда SET поставлена в очередь.");

    // 6. Проверяем остальные команды в COMMAND_MAP (без параметров)
    } else {
        const auto it = SsvcCommandsQueue::COMMAND_MAP.find(commandName);

        if (it != SsvcCommandsQueue::COMMAND_MAP.end()) {
            // Найдена команда без параметров (например, AT, START, STOP, GET и т.д.)
            ESP_LOGI(TAG, "-> Найдена команда '%s'. Вызываем соответствующий метод.", commandName.c_str());

            // Вызываем лямбда-функцию из COMMAND_MAP
            it->second();

            ESP_LOGI(TAG, "<- Команда '%s' поставлена в очередь.", commandName.c_str());
        } else {
            // Неизвестная команда
            ESP_LOGW(TAG, "Неизвестная команда: %s", commandName.c_str());
            const std::string errorMessage = commandName + " - неизвестная команда.";

            // Отправляем ответ об ошибке через MqttBridge
            (void)MqttBridge::getInstance().publish(MQTT_RSP_TOPIC, errorMessage.c_str(), 1, false);
        }
    }
}