/**
 * SSVC Open Connect
 *
 * ... (пропуск заголовка)
 **/

#include "TelemetryService.h"
#include "commons/JsonMemoryLogger.h"
#include "core/SsvcConnector.h"

// Только ради события `telemetry`; в заголовке сокет объявлен forward.
#include <EventSocket.h>

// Интервал обновления телеметрии (2 секунды)
#define TELEMETRY_UPDATE_INTERVAL_MS 2000

#define TELEMETRY_REST_PATH "/rest/telemetry"

static JsonMemoryLogger s_readMemLogger("TelemetryRead", 20);  // лог раз в 20 вызовов

// Регистрируется конструктором: сервис создаётся как обычно в
// SsvcOpenConnect::begin(), поэтому владельцу не нужен отдельный setInstance.
TelemetryService* TelemetryService::_instance = nullptr;

void TelemetryService::readStateToJson(TelemetryState& state, JsonObject& root)
{
    s_readMemLogger.recordBefore();
    _jsonAllocator.reset();
    _jsonAllocator.setTag("TelemetryRead");

    // Десериализуем сохраненный JSON и вставляем его содержимое в корневой объект ответа (документ в PSRAM)
    JsonDocument doc(&_jsonAllocator);
    const DeserializationError error = deserializeJson(doc, state.telemetryJson);
    s_readMemLogger.logAfterParse(doc.overflowed(), _jsonAllocator.usedInternalRam());

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

TelemetryService::TelemetryService(AsyncWebServer *server,
                                    ESP32SvelteKit* sveltekit,
                                   RectificationProcess& rProcess)
    :   _rProcess(rProcess),
        _httpEndpoint([this](TelemetryState& s, JsonObject& r) { readStateToJson(s, r); },
                    [](JsonObject& root, TelemetryState& s, const String&) { return TelemetryState::update(root, s); },
                    this,
                    server,
                    TELEMETRY_REST_PATH,
                    sveltekit->getSecurityManager()),
        _mqttEndpoint([this](TelemetryState& s, JsonObject& r) { readStateToJson(s, r); },
               [](JsonObject& root, TelemetryState& s, const String&) { return TelemetryState::update(root, s); },
               this,
               sveltekit->getMqttClient(),
               TELEMETRY_PUB_TOPIC,
               "", // Топик для подписки: пустой, так как телеметрия Read-Only
               0, // QoS (Quality of Service)
               false)

{
    // Снимок должен видеть именно работающий сервис, а не его отсутствие.
    _instance = this;

    // Сокет берём у ESP32SvelteKit: getSocket() всегда возвращает адрес
    // встроенного объекта, а начинать его раньше времени не нужно — begin()
    // сокета вызывается до создания сервисов (ESP32SvelteKit.cpp).
    _eventSocket = sveltekit->getSocket();

    // Эмиссия привязана к факту изменения: updateTelemetryState() вызывает
    // callUpdateHandlers("InternalTimer") только когда сериализованный снимок
    // отличается от предыдущего. Периодической эмиссии «раз в 2 с» нет —
    // клиент получает кадр ровно на переход, а при подписке — догоняющий снимок.
    addUpdateHandler([this](const String&) { publishTelemetryState(); }, false);

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
    // Ссылка снимается только если она указывает на этот экземпляр: следующий
    // сервис не должен потерять регистрацию из-за удаления предыдущего.
    if (_instance == this) {
        _instance = nullptr;
    }

    // Остановка и удаление таймера (для корректной очистки памяти)
    if (_updateTimer != nullptr) {
        if (xTimerIsTimerActive(_updateTimer) != pdFALSE) {
            xTimerStop(_updateTimer, 0);
        }
        xTimerDelete(_updateTimer, 0);
        _updateTimer = nullptr;
    }
}

void TelemetryService::begin()
{
    if (_eventSocket != nullptr)
    {
        _eventSocket->registerEvent("telemetry");
        // Догоняющая отправка: подписка не должна ждать следующего изменения
        // (снимок может не меняться минутами). Кадр уходит только подписчику,
        // иначе одна подписка рассылала бы полную телеметрию всем клиентам.
        _eventSocket->onSubscribe("telemetry", [this](const String& originId) {
            publishTelemetryState(originId.c_str(), true);
        });
    }

    if (_updateTimer != nullptr) {
        if (xTimerStart(_updateTimer, 0) != pdPASS) {
            ESP_LOGE(TAG, "Failed to start FreeRTOS Telemetry timer!");
        } else {
            ESP_LOGV(TAG, "FreeRTOS Telemetry service update timer started (Interval: %d ms).", 
                     TELEMETRY_UPDATE_INTERVAL_MS);
        }
    } else
        ESP_LOGE(TAG, "TelemetryService: Timer not initialized!");
}

void TelemetryService::publishTelemetryState(const char* originId,
                                             const bool onlyToSameOrigin)
{
    if (_eventSocket == nullptr) {
        // Штатный режим «без сокета»: REST-поведение и таймер не меняются.
        return;
    }

    // Подписка живёт в сокете, поэтому при нуле подключённых клиентов
    // подписчиков заведомо нет. emitEvent в этом случае всё равно сериализует
    // кадр целиком (EventSocket.cpp), а нам это не нужно.
    if (_eventSocket->getConnectedClients() == 0) {
        return;
    }

    // Отдельный PSRAM-аллокатор на вызов: readStateToJson() работает на
    // _jsonAllocator, который делят REST-читатель и таймер, а здесь нужен
    // независимый буфер. Документ живёт до конца emitEvent и освобождается.
    JsonSpiRamAllocator allocator;
    allocator.setTag("TelemetryEvent");
    JsonDocument document(&allocator);
    JsonObject root = document.to<JsonObject>();

    // Снимок читаем под мьютексом StatefulService: строка состояния может
    // меняться таймером. Форма та же, что у REST: {telemetry, status, lastUpdate}.
    bool built = false;
    read([this, &root, &built, &allocator](TelemetryState& state) {
        JsonDocument snapshot(&allocator);
        const DeserializationError error = deserializeJson(snapshot, state.telemetryJson);
        if (error || !snapshot.is<JsonObject>()) {
            ESP_LOGW(TAG, "telemetry event skipped: state is empty or corrupted (%s)",
                     error.c_str());
            return;
        }
        root.set(snapshot.as<JsonObject>());
        root["lastUpdate"] = state.lastUpdateTime;
        built = true;
    });

    if (!built) {
        return;
    }

    // Известное ограничение: вызов идёт из задачи таймера (или WS-задачи при
    // подписке), а не из loop-задачи. Подписки в EventSocket защищены мьютексом,
    // но AsyncWebSocket::binary() из чужой задачи на живом устройстве надо
    // проверить под нагрузкой — переделка задач в этом изменении не планируется.
    _eventSocket->emitEvent("telemetry", root, originId, onlyToSameOrigin);
}

static JsonMemoryLogger s_updateMemLogger("TelemetryUpdate", 0);  // лог при каждом обновлении

void TelemetryService::updateTelemetryState()
{
    s_updateMemLogger.recordBefore();
    _jsonAllocator.reset();
    _jsonAllocator.setTag("TelemetryUpdate");

    // 1. Создаем временный документ для сбора данных (в PSRAM при наличии)
    JsonDocument doc(&_jsonAllocator);

    // 2. RectificationProcess заполняет нужными данными
    // НОВЫЙ ЛОГ: Перед вызовом критической функции (место сбоя)
    ESP_LOGV(TAG, "Calling _rProcess.writeTelemetryTo(root)...");
    const JsonVariant _telemetry = doc["telemetry"].to<JsonVariant>();
    _rProcess.writeTelemetryTo(_telemetry);

    const JsonVariant _status = doc["status"].to<JsonVariant>();
    _rProcess.getStatus(_status);
    _status["uartConnectionError"] =
        SsvcConnector::getConnector().uartCommunicationError;
    ESP_LOGV(TAG, "Finished _rProcess.writeTelemetryTo(root).");

    String newTelemetryJson;
    const size_t size = serializeJson(doc, newTelemetryJson); // Захват размера

    s_updateMemLogger.logAfterParse(doc.overflowed(), _jsonAllocator.usedInternalRam(), size);

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

        _mqttEndpoint.commit();
        ESP_LOGV(TAG, "MQTT Commit triggered. Topic: %s", TELEMETRY_PUB_TOPIC);
    } else {
        // НОВЫЙ ЛОГ: Состояние не изменилось
        ESP_LOGV(TAG, "Telemetry state unchanged.");
    }

    endTransaction();
    // НОВЫЙ ЛОГ: Конец цикла обновления
    ESP_LOGV(TAG, "updateTelemetryState finished. Mutex released.");
}