//
// Created by Admin on 21/10/2025.
//

#ifndef SSVC_OPEN_CONNECT_TELEMETRYSERVICE_H
#define SSVC_OPEN_CONNECT_TELEMETRYSERVICE_H

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

#include "StatefulService.h"
#include <StatefulService.h>
#include <ArduinoJson.h>

#include "core/rectification/RectificationProcess.h" // Источник данных
#include "commons/JsonSpiRamAllocator.h"
#include <Ticker.h> // Для периодического обновления

// Сокет объявлен, а не включён: EventSocket.h тянет ESPAsyncWebServer, а этот
// заголовок подключается в том числе там, где кадр события не нужен.
class EventSocket;

// Максимальный размер JSON-документа для телеметрии (подберите размер под свои данные)
#define TELEMETRY_STATE_DOC_SIZE 2048

#define TELEMETRY_PUB_TOPIC "openconnect/telemetry"

// Структура, содержащая сериализованные данные телеметрии
class TelemetryState
{
    public:
        // Храним полное тело JSON-ответа как строку для эффективного сравнения
        String telemetryJson;
        unsigned long lastUpdateTime = 0;

        // Контракт для обновления (телеметрия Read-Only, обновления не разрешены)
        static StateUpdateResult update(JsonObject &root, TelemetryState &state)
        {
            return StateUpdateResult::UNCHANGED;
        }
    private:
    static constexpr auto TAG = "TelemetryState";
};


class TelemetryService : public StatefulService<TelemetryState>
{
public:
    // Конструктор
    TelemetryService(
            AsyncWebServer *server,
            ESP32SvelteKit* sveltekit,
            RectificationProcess& rProcess
        );

    ~TelemetryService();
    // Не const: в begin() регистрируется событие `telemetry` и подписка на него
    // (нужен неконстантный this для колбэка догоняющей отправки).
    void begin();

    /**
     * @brief Экземпляр сервиса для читателей состояния.
     *
     * Нужен агрегирующему снимку GET /rest/oc/snapshot: он читает телеметрию тем
     * же StatefulService::read(), что и HttpEndpoint, не дёргая сеть и не
     * сериализуя состояние повторно. Сервис создаётся в SsvcOpenConnect::begin()
     * и сам регистрируется в конструкторе — владельцу не нужен новый аксессор.
     */
    static TelemetryService* getInstance() { return _instance; }

private:
    RectificationProcess& _rProcess;
    HttpEndpoint<TelemetryState> _httpEndpoint;
    MqttEndpoint<TelemetryState> _mqttEndpoint;

    JsonSpiRamAllocator _jsonAllocator;
    TimerHandle_t _updateTimer = nullptr;

    // Сокет /ws/events. Владелец — ESP32SvelteKit, поэтому указатель не
    // освобождаем; nullptr допустим (например, сервис собрали без сокета) —
    // тогда событие просто не отправляется.
    EventSocket* _eventSocket = nullptr;

    // Ссылка на единственный созданный экземпляр. Хранится ради читателей
    // состояния (snapshot); nullptr означает «сервис ещё не создан».
    static TelemetryService* _instance;

    void readStateToJson(TelemetryState& state, JsonObject& root);
    static void vUpdateTimerCallback(TimerHandle_t xTimer);

    // Внутренний метод для получения и обновления состояния
    void updateTelemetryState();

    /**
     * @brief Отправляет в /ws/events снимок ровно в форме REST /rest/telemetry.
     *
     * Вызывается из update-handler'а (только при реальном изменении состояния,
     * то есть не чаще ~1 раза в 2 с) и из onSubscribe (догоняющая отправка
     * одному новому подписчику).
     */
    void publishTelemetryState(const char* originId = "", bool onlyToSameOrigin = false);

    static constexpr auto TAG = "TelemetryService";
};


#endif //SSVC_OPEN_CONNECT_TELEMETRYSERVICE_H