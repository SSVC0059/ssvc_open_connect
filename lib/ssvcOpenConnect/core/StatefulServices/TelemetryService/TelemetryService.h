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
#include <Ticker.h> // Для периодического обновления

// Максимальный размер JSON-документа для телеметрии (подберите размер под свои данные)
#define TELEMETRY_STATE_DOC_SIZE 2048

// Структура, содержащая сериализованные данные телеметрии
class TelemetryState
{
    public:
        // Храним полное тело JSON-ответа как строку для эффективного сравнения
        String telemetryJson;
        unsigned long lastUpdateTime = 0;

        // Контракт для чтения состояния (сериализация для REST/WebSocket)
        static void read(const TelemetryState &state, JsonObject &root);

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
            PsychicHttpServer *server,
            ESP32SvelteKit* sveltekit,
            RectificationProcess& rProcess
        );

    ~TelemetryService();
    void begin() const; // Метод для запуска Ticker

private:
    RectificationProcess& _rProcess;
    HttpEndpoint<TelemetryState> _httpEndpoint;

    TimerHandle_t _updateTimer = nullptr;
    static void vUpdateTimerCallback(TimerHandle_t xTimer);

    // Внутренний метод для получения и обновления состояния
    void updateTelemetryState();

    static constexpr auto TAG = "TelemetryService";
};


#endif //SSVC_OPEN_CONNECT_TELEMETRYSERVICE_H