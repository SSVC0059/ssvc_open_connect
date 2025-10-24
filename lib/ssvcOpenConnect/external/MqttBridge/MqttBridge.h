//
// Created by Admin on 24/10/2025.
//

#ifndef SSVC_OPEN_CONNECT_MQTTBRIDGE_H
#define SSVC_OPEN_CONNECT_MQTTBRIDGE_H

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
#include <MqttSettingsService.h>
#include <functional>

/**
 * @brief Ленивый синглтон-обертка для взаимодействия с MQTT-клиентом из MqttSettingsService.
 */
class MqttBridge
{
public:
    using MqttMessageHandler = std::function<void(const String &, const String &, int, bool, bool)>;
    using Subscription = std::pair<std::string, uint8_t>;

    // ------------------------------------------------
    // Управление синглтоном
    // ------------------------------------------------
    static MqttBridge &getInstance(MqttSettingsService *settingsService = nullptr);

    // ------------------------------------------------
    // Методы взаимодействия с MQTT
    // ------------------------------------------------
    uint16_t subscribe(const char *topic, uint8_t qos = 0) const;
    uint16_t publish(const char *topic, const char *payload, uint8_t qos = 0, bool retained = false) const;
    uint16_t publish(const char *topic, const std::string& payload, uint8_t qos = 0, bool retained = false) const;
    void setMessageHandler(const MqttMessageHandler& handler);

    /**
     * @brief Регистрирует топик для автоматической переподписки при каждом подключении.
     * @param topic Топик для подписки.
     * @param qos Качество обслуживания.
     */
    void registerSubscription(const std::string& topic, uint8_t qos);

private:
    MqttSettingsService *_settingsService;
    MqttMessageHandler _userHandler;

    std::vector<Subscription> _subscriptions;

    explicit MqttBridge(MqttSettingsService *settingsService);

    void onMqttConnect(bool sessionPresent) const;

    void onMqttMessage(char *topic, char *payload, int retain, int qos, bool dup);

    static constexpr auto TAG = "MqttBridge";
};

#endif //SSVC_OPEN_CONNECT_MQTTBRIDGE_H