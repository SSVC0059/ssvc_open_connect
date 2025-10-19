//
// Created by demoncat on 09.05.25.
//

#ifndef ALERTMANAGER_H
#define ALERTMANAGER_H

#ifndef ALERT_MANAGER_H
#define ALERT_MANAGER_H

#include <Arduino.h>
#include <functional>
#include <vector>
#include <map>
#include <queue>
#include <mutex>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

class Watchdog {
private:
    Watchdog();
    ~Watchdog();

    // Запрет копирования
    Watchdog(const Watchdog&) = delete;
    Watchdog& operator=(const Watchdog&) = delete;

    struct Handler {
        std::function<void(const String&, const String&)> callback;
        int priority;

        bool operator<(const Handler& other) const {
            return priority < other.priority;
        }
    };

    struct AsyncEvent {
        String eventName;
        String jsonData;
    };

    static void asyncTask(void* params);
    void processAsyncEvents();
    void processEvent(const String& eventName, const String& jsonData);

    std::mutex handlersMutex;
    std::mutex queueMutex;
    std::map<String, std::vector<Handler>> eventHandlers;
    std::queue<AsyncEvent> eventQueue;
    TaskHandle_t asyncTaskHandle = nullptr;

public:
    static Watchdog& manager();

    void subscribe(const String& eventName,
                  std::function<void(const String&, const String&)> handler,
                  int priority = 0);

    void publish(const String& eventName, const String& jsonData = "");
    void publishAsync(const String& eventName, const String& jsonData = "");
    void unsubscribeAll(const String& eventName);
    size_t getQueueSize();
};

#endif // ALERT_MANAGER_H


#endif //ALERTMANAGER_H
