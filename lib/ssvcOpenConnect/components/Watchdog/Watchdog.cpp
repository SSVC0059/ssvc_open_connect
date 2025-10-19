//
// Created by demoncat on 09.05.25.
//
#include "Watchdog.h"

/*
// Подписка с разными приоритетами
AlertManager::manager().subscribe("networkEvent",
    [](const String&, const String& json) {
        // Обработка с высоким приоритетом
    }, 10); // Высокий приоритет

AlertManager::manager().subscribe("networkEvent",
    [](const String&, const String& json) {
        // Фоновая обработка
    }, -5); // Низкий приоритет

// Синхронная публикация (блокирующая)
AlertManager::manager().publish("networkEvent", "{\"status\":\"connected\"}");

// Асинхронная публикация (не блокирующая)
AlertManager::manager().publishAsync("sensorUpdate", "{\"temp\":22.5}");

// В loop() можно добавить мониторинг
void loop() {
    Serial.printf("Queue size: %d\n", AlertManager::getInstance().getQueueSize());
    delay(1000);
}
*/

Watchdog::Watchdog() {
    // Создаем задачу для асинхронной обработки
    xTaskCreatePinnedToCore(
        asyncTask,
        "AlertManager",
        4096,
        this,
        1,
        &asyncTaskHandle,
        APP_CPU_NUM
    );
}

Watchdog::~Watchdog() {
    if (asyncTaskHandle) {
        vTaskDelete(asyncTaskHandle);
    }
}

void Watchdog::asyncTask(void* params) {
    const auto manager = static_cast<Watchdog*>(params);
    while (true) {
        manager->processAsyncEvents();
        vTaskDelay(10 / portTICK_PERIOD_MS); // Небольшая задержка
    }
}

void Watchdog::processAsyncEvents() {
    std::queue<AsyncEvent> localQueue;

    {
        // Быстро копируем события в локальную очередь
        std::lock_guard<std::mutex> lock(queueMutex);
        if (eventQueue.empty()) return;
        localQueue = std::move(eventQueue);
    }

    // Обрабатываем события вне блокировки
    while (!localQueue.empty()) {
        auto event = localQueue.front();
        localQueue.pop();
        processEvent(event.eventName, event.jsonData);
    }
}

void Watchdog::processEvent(const String& eventName, const String& jsonData) {
    std::vector<Handler> handlersCopy;

    {
        std::lock_guard<std::mutex> lock(handlersMutex);
        const auto it = eventHandlers.find(eventName);
        if (it == eventHandlers.end()) return;
        handlersCopy = it->second;
    }

    // Сортируем по приоритету (высокий приоритет первый)
    std::sort(handlersCopy.rbegin(), handlersCopy.rend());

    // Вызываем обработчики
    for (auto& handler : handlersCopy) {
        handler.callback(eventName, jsonData);
    }
}

Watchdog& Watchdog::manager() {
    static Watchdog instance;
    return instance;
}

void Watchdog::subscribe(const String& eventName,
                           std::function<void(const String&, const String&)> handler,
                           int priority) {
    std::lock_guard<std::mutex> lock(handlersMutex);
    eventHandlers[eventName].push_back({handler, priority});
}

void Watchdog::publish(const String& eventName, const String& jsonData) {
    processEvent(eventName, jsonData);
}

void Watchdog::publishAsync(const String& eventName, const String& jsonData) {
    std::lock_guard<std::mutex> lock(queueMutex);
    eventQueue.push({eventName, jsonData});
}

void Watchdog::unsubscribeAll(const String& eventName) {
    std::lock_guard<std::mutex> lock(handlersMutex);
    eventHandlers.erase(eventName);
}

size_t Watchdog::getQueueSize() {
    std::lock_guard<std::mutex> lock(queueMutex);
    return eventQueue.size();
}