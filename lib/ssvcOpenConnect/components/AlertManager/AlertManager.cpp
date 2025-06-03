//
// Created by demoncat on 09.05.25.
//
#include "AlertManager.h"

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

AlertManager::AlertManager() {
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

AlertManager::~AlertManager() {
    if (asyncTaskHandle) {
        vTaskDelete(asyncTaskHandle);
    }
}

[[noreturn]] void AlertManager::asyncTask(void* params) {
    const auto manager = static_cast<AlertManager*>(params);
    while (true) {
        manager->processAsyncEvents();
        vTaskDelay(10 / portTICK_PERIOD_MS); // Небольшая задержка
    }
}

void AlertManager::processAsyncEvents() {
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

void AlertManager::processEvent(const String& eventName, const String& jsonData) {
    std::vector<Handler> handlersCopy;

    {
        std::lock_guard<std::mutex> lock(handlersMutex);
        auto it = eventHandlers.find(eventName);
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

AlertManager& AlertManager::manager() {
    static AlertManager instance;
    return instance;
}

void AlertManager::subscribe(const String& eventName,
                           std::function<void(const String&, const String&)> handler,
                           int priority) {
    std::lock_guard<std::mutex> lock(handlersMutex);
    eventHandlers[eventName].push_back({handler, priority});
}

void AlertManager::publish(const String& eventName, const String& jsonData) {
    processEvent(eventName, jsonData);
}

void AlertManager::publishAsync(const String& eventName, const String& jsonData) {
    std::lock_guard<std::mutex> lock(queueMutex);
    eventQueue.push({eventName, jsonData});
}

void AlertManager::unsubscribeAll(const String& eventName) {
    std::lock_guard<std::mutex> lock(handlersMutex);
    eventHandlers.erase(eventName);
}

size_t AlertManager::getQueueSize() {
    std::lock_guard<std::mutex> lock(queueMutex);
    return eventQueue.size();
}