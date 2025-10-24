#include "MqttBridge.h"

// Глобальная статическая переменная для синглтона
static MqttBridge *s_instance = nullptr;

/**
 * @brief Приватный конструктор для синглтона.
 */
MqttBridge::MqttBridge(MqttSettingsService *settingsService) : _settingsService(settingsService)
{
    if (_settingsService == nullptr)
    {
        ESP_LOGE(TAG, "MqttSettingsService must be provided on first instantiation.");
        return;
    }

    PsychicMqttClient *client = _settingsService->getMqttClient();


    client->onMessage(
        [](char *topic, char *payload, int retain, int qos, bool dup) {
            getInstance().onMqttMessage(topic, payload, retain, qos, dup);
        }
    );

    // Для onConnect можно использовать лямбду с захватом this, если она работает в вашей среде
    client->onConnect(
        [this](const bool sessionPresent) {
            this->onMqttConnect(sessionPresent);
        }
    );

    ESP_LOGD(TAG, "MqttBridge initialized.");
}

MqttBridge &MqttBridge::getInstance(MqttSettingsService *settingsService)
{
    if (s_instance == nullptr)
    {
        s_instance = new MqttBridge(settingsService);
    }
    return *s_instance;
}

/**
 * @brief Подписаться на указанный топик.
 */
uint16_t MqttBridge::subscribe(const char *topic, uint8_t qos) const
{
    if (!_settingsService || !_settingsService->isConnected())
    {
        ESP_LOGW(TAG, "Cannot subscribe to %s: MQTT client is not connected.", topic);
        return 0;
    }
    ESP_LOGD(TAG, "Subscribing to topic: %s with QoS %d", topic, qos);
    // Используем PsychicMqttClient, который управляется сервисом
    return _settingsService->getMqttClient()->subscribe(topic, qos);
}

/**
 * @brief Публикация сообщения в указанный топик.
 */
uint16_t MqttBridge::publish(const char *topic, const char *payload, uint8_t qos, bool retained) const
{
    if (!_settingsService || !_settingsService->isConnected())
    {
        ESP_LOGV(TAG, "Cannot publish to %s: MQTT client is not connected.", topic);
        return 0;
    }
    // Используем PsychicMqttClient, который управляется сервисом
    return _settingsService->getMqttClient()->publish(topic, qos, retained, payload);
}

uint16_t MqttBridge::publish(const char *topic, const std::string& payload, uint8_t qos, bool retained) const
{
    // ✅ Вызываем оригинальный метод, используя payload.c_str()
    return this->publish(topic, payload.c_str(), qos, retained);
}

/**
 * @brief Установить пользовательский обработчик входящих сообщений.
 */
void MqttBridge::setMessageHandler(const MqttMessageHandler& handler)
{
    _userHandler = handler;
}

/**
 * @brief Регистрирует топик для автоматической переподписки при каждом подключении.
 */
void MqttBridge::registerSubscription(const std::string& topic, uint8_t qos)
{
    // Проверяем, не был ли этот топик уже зарегистрирован (опционально, но желательно)
    for (const auto& sub : _subscriptions) {
        if (sub.first == topic && sub.second == qos) {
            ESP_LOGW(TAG, "Subscription already registered: %s (QoS %d)", topic.c_str(), qos);
            return;
        }
    }

    // Добавляем новую подписку
    _subscriptions.push_back({topic, qos});
    ESP_LOGD(TAG, "Registered topic for auto-resubscription: %s (QoS %d)", topic.c_str(), qos);
}

void MqttBridge::onMqttConnect(const bool sessionPresent) const
{
    ESP_LOGD(TAG, "MQTT client connected (sessionPresent: %s). Starting subscriptions.", sessionPresent ? "true" : "false");
    for (const auto& sub : _subscriptions) {
        const char* topic = sub.first.c_str();
        const uint8_t qos = sub.second;

        const uint16_t msgId = subscribe(topic, qos);

        if (msgId == 0) {
            ESP_LOGW(TAG, "Failed to resubscribe to '%s' (QoS %d)", topic, qos);
        } else {
            ESP_LOGD(TAG, "Resubscribed to '%s' (QoS %d, MsgId: %d)", topic, qos, msgId);
        }
    }
}

/**
 * @brief Внутренний обработчик входящих MQTT-сообщений.
 */
void MqttBridge::onMqttMessage(char *topic, char *payload, int retain, int qos, bool dup)
{
    if (_userHandler)
    {

        String topicStr(topic);
        String payloadStr(payload);

        ESP_LOGD(TAG, "Message received on topic: %s", topic);
        _userHandler(topicStr, payloadStr, qos, retain > 0, dup);
    }
}