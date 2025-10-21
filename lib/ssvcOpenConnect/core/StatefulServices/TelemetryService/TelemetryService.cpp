/**
 * SSVC Open Connect
 *
 * ... (пропуск заголовка)
 **/

#include "TelemetryService.h"

// Интервал обновления телеметрии (2 секунды)
#define TELEMETRY_UPDATE_INTERVAL_MS 2000
#define TELEMETRY_REST_PATH "/rest/telemetry"

void TelemetryState::read(const TelemetryState &state, JsonObject &root)

{
    // Десериализуем сохраненный JSON и вставляем его содержимое в корневой объект ответа
    JsonDocument doc;
    const DeserializationError error = deserializeJson(doc, state.telemetryJson);
    if (!error && doc.is<JsonObject>()) {
        // Вставляем все содержимое сохраненного JSON (telemetry + status) в корень ответа
        root.set(doc.as<JsonObject>());
    } else {
        ESP_LOGE(TAG, "Deserialization failed: %s or state empty. Size: %zu",
                 error.c_str(), state.telemetryJson.length());
        root["error"] = "Telemetry state empty or corrupted";
    }
    root["lastUpdate"] = state.lastUpdateTime;
}

// Static FreeRTOS Timer Callback: получает указатель на объект и вызывает метод экземпляра
void TelemetryService::vUpdateTimerCallback(TimerHandle_t xTimer)
{
    const auto self = static_cast<TelemetryService*>(pvTimerGetTimerID(xTimer));
    if (self) {
        self->updateTelemetryState();
    } else {
        // НОВЫЙ ЛОГ: Критическая ошибка таймера
        ESP_LOGE(TAG, "Timer callback failed to retrieve 'self' pointer!");
    }
}

TelemetryService::TelemetryService(PsychicHttpServer *server,
                                    ESP32SvelteKit* sveltekit,
                                   RectificationProcess& rProcess)
    : _rProcess(rProcess),
      _httpEndpoint(TelemetryState::read,
                    TelemetryState::update,
                    this,
                    server,
                    TELEMETRY_REST_PATH,
                    sveltekit->getSecurityManager())
{
    // Создание FreeRTOS Timer
    _updateTimer = xTimerCreate(
        "TelemetryUpdateTimer",
        pdMS_TO_TICKS(TELEMETRY_UPDATE_INTERVAL_MS),
        pdTRUE,
        this,
        vUpdateTimerCallback
    );

    if (_updateTimer == nullptr) {
        ESP_LOGE(TAG, "Failed to create FreeRTOS Telemetry timer!");
    }
    _httpEndpoint.begin();
}

TelemetryService::~TelemetryService()
{
    // Остановка и удаление таймера (для корректной очистки памяти)
    if (_updateTimer != nullptr) {
        if (xTimerIsTimerActive(_updateTimer) != pdFALSE) {
            xTimerStop(_updateTimer, 0);
        }
        xTimerDelete(_updateTimer, 0);
        _updateTimer = nullptr;
    }
}

void TelemetryService::begin() const
{
    if (_updateTimer != nullptr) {
        if (xTimerStart(_updateTimer, 0) != pdPASS) {
            ESP_LOGE(TAG, "Failed to start FreeRTOS Telemetry timer!");
        } else {
            // Улучшенный ЛОГ: Указываем интервал
            ESP_LOGV(TAG, "FreeRTOS Telemetry service update timer started (Interval: %d ms).", 
                     TELEMETRY_UPDATE_INTERVAL_MS);
        }
    } else
        ESP_LOGE(TAG, "TelemetryService: Timer not initialized!");
}

void TelemetryService::updateTelemetryState()
{
    // 1. Создаем временный документ для сбора данных
    JsonDocument doc;

    // 2. RectificationProcess заполняет нужными данными
    // НОВЫЙ ЛОГ: Перед вызовом критической функции (место сбоя)
    ESP_LOGV(TAG, "Calling _rProcess.writeTelemetryTo(root)...");
    const JsonVariant _telemetry = doc["telemetry"].to<JsonVariant>();
    _rProcess.writeTelemetryTo(_telemetry);
    // НОВЫЙ ЛОГ: После вызова критической функции (если сюда дойдет)
    ESP_LOGV(TAG, "Finished _rProcess.writeTelemetryTo(root).");


    // 3. Сериализуем документ в строку
    String newTelemetryJson;
    const size_t size = serializeJson(doc, newTelemetryJson); // Захват размера

    // НОВЫЙ ЛОГ: Размер JSON
    ESP_LOGV(TAG, "Serialized JSON size: %zu bytes. Old size: %zu bytes.", 
             size, _state.telemetryJson.length());

    // 4. Захват мьютекса и обновление состояния
    beginTransaction();
    // НОВЫЙ ЛОГ: Захват мьютекса
    ESP_LOGV(TAG, "Mutex acquired.");


    // Проверка изменения состояния: сравниваем новую строку с сохраненной
    if (_state.telemetryJson != newTelemetryJson)
    {
        // НОВЫЙ ЛОГ: Обнаружено изменение
        ESP_LOGV(TAG, "Telemetry state changed! Updating and calling handlers.");

        _state.telemetryJson = newTelemetryJson;
        _state.lastUpdateTime = millis();

        // 1. Определяем, что состояние изменилось
        auto result = StateUpdateResult::CHANGED;

        // 2. Вызываем хуки (если есть)
        this->callHookHandlers("InternalTimer", result);

        // 3. Вызываем обработчики обновления (триггер для WebSockets/EventSockets)
        this->callUpdateHandlers("InternalTimer");
    } else {
        // НОВЫЙ ЛОГ: Состояние не изменилось
        ESP_LOGV(TAG, "Telemetry state unchanged.");
    }

    endTransaction();
    // НОВЫЙ ЛОГ: Конец цикла обновления
    ESP_LOGV(TAG, "updateTelemetryState finished. Mutex released.");
}