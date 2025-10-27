//
// Created by Admin on 16/10/2025.
//

#include "AlarmMonitor.h"

AlarmMonitor& AlarmMonitor::getInstance() {
    ESP_LOGV(TAG, "Singleton instance retrieved.");
    static AlarmMonitor instance;
    return instance;
}

void AlarmMonitor::initialize(AlarmThresholdService* service) {
    _thresholdService = service;
    _last_alarm_states.clear();
    if (_thresholdService) {
        // Лямбда-функция для передачи в addUpdateHandler
        auto update_cb = [this](const String& originId) {
            this->onThresholdsUpdated(originId);
        };
        // Сохраняем ID, чтобы иметь возможность отписаться (хотя в Singleton это редко нужно)
        _updateHandlerId = _thresholdService->addUpdateHandler(update_cb, false);

        ESP_LOGI(TAG, "AlarmMonitor subscribed to Threshold Service updates (ID: %zu).", _updateHandlerId);
    }

    ESP_LOGI(TAG, "AlarmMonitor initialized with threshold service.");
}

void AlarmMonitor::subscribe(IAlarmSubscriber* subscriber) {
    if (subscriber == nullptr) {
        ESP_LOGW(TAG, "Attempted to subscribe a NULL pointer.");
        return;
    }

    // Проверяем, не подписан ли уже
    const bool already_subscribed = std::find(_subscribers.begin(), _subscribers.end(), subscriber) != _subscribers.end();

    if (!already_subscribed) {
        _subscribers.push_back(subscriber);
        // Логируем успешную подписку и общее количество
        ESP_LOGI(TAG, "Subscriber 0x%p successfully added. Total subscribers: %zu.",
                 static_cast<void*>(subscriber), _subscribers.size());
    } else {
        ESP_LOGD(TAG, "Subscriber 0x%p is already subscribed. Skipping.", static_cast<void*>(subscriber));
    }
}

// Файл: AlarmMonitor.cpp

// Добавьте этот метод, который будет вызываться при изменении настроек
void AlarmMonitor::onThresholdsUpdated(const String& originId) {
    ESP_LOGI(TAG, "Threshold settings changed (Origin: %s). Re-checking all sensors immediately.", originId.c_str());

    for (IAlarmSubscriber* subscriber : _subscribers) {
        subscriber->forceResetAlarm(); // <-- ВЫЗЫВАЕМ НОВЫЙ МЕТОД
    }
    // Обнуляем предыдущие состояния, чтобы гарантировать, что НОВЫЙ уровень сработает
    // даже если он совпадает со старым, но был пропущен из-за таймаута.
    // Если этого не сделать, в _last_alarm_states могут остаться старые уровни,
    // и новый уровень (например, CRITICAL) не сработает, если он уже был CRITICAL
    // до изменения порогов.
    _last_alarm_states.clear();

    // Принудительно запускаем проверку
    checkAllSensors();
}


void AlarmMonitor::unsubscribe(IAlarmSubscriber* subscriber) {
    if (subscriber == nullptr) {
        ESP_LOGW(TAG, "Attempted to unsubscribe a NULL pointer.");
        return;
    }

    const size_t old_size = _subscribers.size();
    // Используем erase-remove idiom для удаления указателя из вектора
    _subscribers.erase(
        std::remove(_subscribers.begin(), _subscribers.end(), subscriber),
        _subscribers.end()
    );

    // Логируем результат отписки
    if (_subscribers.size() < old_size) {
        ESP_LOGI(TAG, "Subscriber 0x%p successfully removed. Total subscribers: %zu.",
                 static_cast<void*>(subscriber), _subscribers.size());
    } else {
        ESP_LOGD(TAG, "Subscriber 0x%p was not found in the list.", static_cast<void*>(subscriber));
    }
}


void AlarmMonitor::notifySubscribers(const AlarmEvent& event) const
{
    // Информационный лог о событии тревоги перед рассылкой
    ESP_LOGW(TAG, "--- ALARM EVENT TRIGGERED --- Address: %s, Value: %.2f, Level: %d. Notifying %zu subscribers.",
             SensorManager::addressToString(event.sensor->getAddress()).c_str(),
             event.current_value,
             static_cast<int>(event.level),
             _subscribers.size());

    // Итерируемся по всем подписчикам и вызываем их метод onAlarmEvent
    for (IAlarmSubscriber* subscriber : _subscribers) {
        if (subscriber) {
            // Подробный лог о рассылке каждому подписчику
            ESP_LOGV(TAG, "Notifying subscriber 0x%p about event for sensor %s.",
                     (void*)subscriber, SensorManager::addressToString(event.sensor->getAddress()).c_str());
            subscriber->onAlarm(event);
        }
    }
}

void AlarmMonitor::checkAllSensors() {
    if (!_thresholdService) {
        ESP_LOGE(TAG, "Cannot check sensors: Threshold Service is NULL.");
        return;
    }

    // Подробный лог о начале цикла проверки
    ESP_LOGI(TAG, "Starting sensor check cycle.");

    // 1. Получаем доступ к текущим настройкам порогов
    _thresholdService->read([&](const AlarmThresholdsState& thresholdsState) {

        // 2. Получаем все датчики
        const SensorManager& sm = SensorManager::getInstance();
        const auto& all_sensors = sm.getAllSensors();
        ESP_LOGI(TAG, "Processing %zu sensors.", all_sensors.size());

        for (const auto& pair : all_sensors) {
            const std::string& addr_str = pair.first;
            AbstractSensor* sensor = pair.second;

            // 3. Проверяем, есть ли для этого датчика настройки
            if (thresholdsState.sensor_thresholds.count(addr_str) == 0) {
                ESP_LOGI(TAG, "Sensor %s skipped: No threshold settings found.", addr_str.c_str());
                continue; // Настроек нет, пропускаем
            }

            const ThresholdSettings& settings = thresholdsState.sensor_thresholds.at(addr_str);
            if (!settings.enabled) {
                ESP_LOGI(TAG, "Sensor %s skipped: Monitoring is disabled.", addr_str.c_str());
                continue; // Мониторинг для датчика выключен
            }
            // Подробный лог с настройками
            ESP_LOGI(TAG, "Sensor %s: Thresh (Crit:%.2f, Dang:%.2f, Min:%.2f).",
                     addr_str.c_str(), settings.critical_threshold, settings.dangerous_threshold, settings.min_threshold);
            // 4. Сравнение
            const float current_value = sensor->getData();


            // 4.1. Проверка валидности данных
         if (!sensor->isDataValid()) {

             // Защита от ложной тревоги на старте:
             // Если датчика еще нет в списке _last_alarm_states, это первый цикл.
             const bool is_first_check = (_last_alarm_states.count(addr_str) == 0);

             if (is_first_check) {
                 // Инициализируем состояние как NORMAL, но не генерируем событие.
                 _last_alarm_states[addr_str] = AlarmLevel::NORMAL;
                 ESP_LOGI(TAG, "Sensor %s is in initial state (non-valid data). Skipping alarm check.", addr_str.c_str());
                 return; // Пропускаем, чтобы избежать ложной тревоги MINIMUM
             }

             // Если код дошел сюда, это СБОЙ ВО ВРЕМЯ РАБОТЫ.
             auto new_level = AlarmLevel::CRITICAL;
             const AlarmLevel last_level = _last_alarm_states.count(addr_str) ?
                              _last_alarm_states.at(addr_str) :
                              AlarmLevel::NORMAL;

             if (new_level != last_level) {
                 ESP_LOGE(TAG, "SENSOR FAILURE DETECTED! Sensor %s transition: %d -> %d. Value: %.2f (Non-valid data)",
                          addr_str.c_str(), static_cast<int>(last_level), static_cast<int>(new_level), current_value);

                 _last_alarm_states[addr_str] = new_level;

                 const AlarmEvent event = {
                     sensor,
                     current_value,
                     FAILURE_THRESHOLD_VALUE, // <-- Используем символическое значение
                     new_level,
                     time(nullptr)
                 };
                 notifySubscribers(event);
             }
             continue; // Завершаем обработку
         }

            auto new_level = AlarmLevel::NORMAL;
            float threshold_crossed = 0.0f;

            // Логируем текущее значение
            ESP_LOGI(TAG, "Sensor %s read value: %.2f.", addr_str.c_str(), current_value);

            // --- Логика определения уровня тревоги ---
            if (current_value >= settings.critical_threshold) {
                new_level = AlarmLevel::CRITICAL;
                threshold_crossed = settings.critical_threshold;
            } else if (current_value >= settings.dangerous_threshold) {
                new_level = AlarmLevel::DANGEROUS;
                threshold_crossed = settings.dangerous_threshold;
            } else if (current_value <= settings.min_threshold) {
                new_level = AlarmLevel::MIN;
                threshold_crossed = settings.min_threshold;
            }
            // ----------------------------------------

            // 5. Проверяем, изменился ли статус тревоги
            const AlarmLevel last_level = _last_alarm_states.count(addr_str) ?
                                          _last_alarm_states.at(addr_str) :
                                          AlarmLevel::NORMAL; // Предполагаем NORMAL, если не было предыдущего состояния

            // Логируем переход состояния
            if (new_level != last_level) {
                ESP_LOGI(TAG, "STATUS CHANGE! Sensor %s transition: %d -> %d. Value: %.2f (Crossed: %.2f)",
                         addr_str.c_str(), static_cast<int>(last_level), static_cast<int>(new_level), current_value, threshold_crossed);

                _last_alarm_states[addr_str] = new_level; // Обновляем состояние

                // Генерируем событие и уведомляем подписчиков
                AlarmEvent event = {
                    sensor,
                    current_value,
                    threshold_crossed,
                    new_level,
                    time(nullptr)
                };
                notifySubscribers(event);

            } else {
                // Подробный лог, если состояние не изменилось
                ESP_LOGI(TAG, "Sensor %s: State remains %d (Value: %.2f). No notification sent.",
                         addr_str.c_str(), static_cast<int>(new_level), current_value);
            }
        }
    });

    ESP_LOGD(TAG, "Sensor check cycle finished.");
}