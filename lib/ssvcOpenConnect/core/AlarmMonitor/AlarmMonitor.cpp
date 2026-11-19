//
// Created by Admin on 16/10/2025.
//

#include "AlarmMonitor.h"

#include <cstdio>
#include <cstring>

// Только ради события `alarm`; в заголовке сокет объявлен forward.
#include <EventSocket.h>

namespace {

// Строки — часть контракта REST /rest/oc/hardware-faults и события `alarm`.
// Должны совпадать с hardwareFaultCodeKey() в OpenConnectHandler.cpp; копия
// здесь потому, что та функция — приватная деталь обработчика, а тянуть его
// заголовок в AlarmMonitor ради пяти строк не стоит.
const char* hardwareFaultCodeKey(const HardwareFaultCode code) {
  switch (code) {
    case HardwareFaultCode::NONE: return "none";
    case HardwareFaultCode::I2C_NACK: return "i2c_nack";
    case HardwareFaultCode::I2C_BUS_DOWN: return "i2c_bus_down";
    case HardwareFaultCode::DEVICE_NOT_PRESENT: return "device_not_present";
    case HardwareFaultCode::UART_LINK_LOST: return "uart_link_lost";
    default: return "unknown";
  }
}

const char* alarmLevelKey(const AlarmLevel level) {
  switch (level) {
    case AlarmLevel::MIN: return "min";
    case AlarmLevel::DANGEROUS: return "dangerous";
    case AlarmLevel::CRITICAL: return "critical";
    case AlarmLevel::NORMAL:
    default: return "normal";
  }
}

class MonitorLock {
public:
  explicit MonitorLock(SemaphoreHandle_t lock) : _lock(lock) {
    if (_lock != nullptr) {
      xSemaphoreTakeRecursive(_lock, portMAX_DELAY);
    }
  }

  ~MonitorLock() {
    if (_lock != nullptr) {
      xSemaphoreGiveRecursive(_lock);
    }
  }

private:
  SemaphoreHandle_t _lock;
};

} // namespace

std::string AlarmMonitor::makeHardwareStateKey(const char* device_role, const uint8_t i2c_addr) {
  char buf[96];
  snprintf(buf, sizeof(buf), "hw:%s:0x%02X", device_role ? device_role : "?",
           static_cast<unsigned>(i2c_addr));
  return std::string(buf);
}

AlarmMonitor::AlarmMonitor() : _lock(xSemaphoreCreateRecursiveMutex()) {}

void AlarmMonitor::setEventSocket(EventSocket* socket) {
  // Сеттер только запоминает указатель: регистрация имени события — в
  // initialize(), которое владелец вызывает после установки сокета.
  _eventSocket = socket;
}

void AlarmMonitor::raiseHardwareFault(const HardwareFaultCode code, const uint8_t i2c_addr,
                                      const char* device_role) {
  const std::string key = makeHardwareStateKey(device_role, i2c_addr);
  constexpr AlarmLevel new_level = AlarmLevel::CRITICAL;
  bool shouldNotify = false;
  {
    MonitorLock lock(_lock);
    const AlarmLevel last =
        _last_hw_alarm_states.count(key) ? _last_hw_alarm_states.at(key) : AlarmLevel::NORMAL;
    _hw_fault_active_codes[key] = code; // always keep the code up to date
    if (last != new_level) {
      _last_hw_alarm_states[key] = new_level;
      shouldNotify = true;
    }
  }

  // Код сбоя мог измениться без смены уровня (NACK → BUS_DOWN): тогда
  // notifySubscribers() не вызывается, а поток событий обязан увидеть новый
  // код. Отправка идёт до раннего выхода; повтор того же кода на каждом
  // периодическом опросе шины отсекает таблица последних отправленных.
  AlarmEvent state_event{};
  state_event.source_kind = AlarmSourceKind::HARDWARE_FAULT;
  state_event.level = new_level;
  state_event.hw_code = code;
  state_event.hw_i2c_address = i2c_addr;
  state_event.hw_device_role = device_role;
  publishAlarmEvent(state_event);

  if (!shouldNotify) {
      return;
  }

  AlarmEvent ev{};
  ev.source_kind = AlarmSourceKind::HARDWARE_FAULT;
  ev.sensor = nullptr;
  ev.level = new_level;
  ev.hw_code = code;
  ev.hw_i2c_address = i2c_addr;
  ev.hw_device_role = device_role;
  ev.timestamp = time(nullptr);
  ev.current_value = static_cast<float>(static_cast<unsigned>(code));
  ev.threshold_value = 0.f;
  notifySubscribers(ev);
}

void AlarmMonitor::clearHardwareFault(const char* device_role, const uint8_t i2c_addr) {
  const std::string key = makeHardwareStateKey(device_role, i2c_addr);
  bool shouldNotify = false;
  {
    MonitorLock lock(_lock);
    _hw_fault_active_codes.erase(key);
    if (_last_hw_alarm_states.count(key) && _last_hw_alarm_states.at(key) != AlarmLevel::NORMAL) {
      _last_hw_alarm_states[key] = AlarmLevel::NORMAL;
      shouldNotify = true;
    }
  }
  // Снятие — тоже переход: клиент должен увидеть active=false, даже если
  // уровень уже был NORMAL (тогда notifySubscribers не вызывается). Таблица
  // последних отправленных не даст повторно отправить то же снятие.
  AlarmEvent state_event{};
  state_event.source_kind = AlarmSourceKind::HARDWARE_FAULT;
  state_event.level = AlarmLevel::NORMAL;
  state_event.hw_code = HardwareFaultCode::NONE;
  state_event.hw_i2c_address = i2c_addr;
  state_event.hw_device_role = device_role;
  publishAlarmEvent(state_event);

  if (!shouldNotify) {
      return;
  }

  AlarmEvent ev{};
  ev.source_kind = AlarmSourceKind::HARDWARE_FAULT;
  ev.sensor = nullptr;
  ev.level = AlarmLevel::NORMAL;
  ev.hw_code = HardwareFaultCode::NONE;
  ev.hw_i2c_address = i2c_addr;
  ev.hw_device_role = device_role;
  ev.timestamp = time(nullptr);
  ev.current_value = 0.f;
  ev.threshold_value = 0.f;
  notifySubscribers(ev);
}

std::vector<AlarmMonitor::HardwareFaultEntry> AlarmMonitor::getActiveHardwareFaults() const {
  std::vector<HardwareFaultEntry> result;
  MonitorLock lock(_lock);
  for (const auto& kv : _last_hw_alarm_states) {
    if (kv.second == AlarmLevel::NORMAL) {
      continue;
    }
    const std::string& key = kv.first;
    // Key format: "hw:{role}:0x{hex_addr}"
    if (key.size() < 4 || key.compare(0, 3, "hw:") != 0) {
      continue;
    }
    const size_t roleEnd = key.rfind(":0x");
    if (roleEnd == std::string::npos || roleEnd < 3) {
      continue;
    }
    HardwareFaultEntry entry;
    entry.role = key.substr(3, roleEnd - 3);
    const std::string addrHex = key.substr(roleEnd + 3);
    entry.i2c_addr = static_cast<uint8_t>(strtoul(addrHex.c_str(), nullptr, 16));
    const auto it = _hw_fault_active_codes.find(key);
    entry.code = (it != _hw_fault_active_codes.end()) ? it->second : HardwareFaultCode::NONE;
    result.push_back(std::move(entry));
  }
  return result;
}

std::vector<std::string> AlarmMonitor::getKnownHardwareRoles() const {
  std::vector<std::string> roles;
  MonitorLock lock(_lock);
  roles.reserve(_last_hw_alarm_states.size());
  for (const auto& it : _last_hw_alarm_states) {
    const std::string& key = it.first;
    const size_t prefix = key.find("hw:");
    if (prefix != 0) {
      continue;
    }
    const size_t roleStart = 3;
    const size_t roleEnd = key.rfind(":0x");
    if (roleEnd == std::string::npos || roleEnd <= roleStart) {
      continue;
    }
    const std::string role = key.substr(roleStart, roleEnd - roleStart);
    if (!role.empty() &&
        std::find(roles.begin(), roles.end(), role) == roles.end()) {
      roles.push_back(role);
    }
  }
  return roles;
}

AlarmMonitor& AlarmMonitor::getInstance() {
    ESP_LOGV(TAG, "Singleton instance retrieved.");
    static AlarmMonitor instance;
    return instance;
}

void AlarmMonitor::initialize(AlarmThresholdService* service) {
    {
        MonitorLock lock(_lock);
        _thresholdService = service;
        _last_alarm_states.clear();
        _last_hw_alarm_states.clear();
        _hw_fault_active_codes.clear();
        // Новая инициализация — новый поток событий: старые «последние
        // отправленные» не должны подавлять первые реальные тревоги.
        _lastEmittedAlarmCount = 0;
        _nextLastEmittedAlarmSlot = 0;
        for (auto& entry : _lastEmittedAlarmStates) {
            entry.key[0] = '\0';
            entry.state = 0;
        }
    }
    if (_eventSocket != nullptr) {
        // Имя регистрируется до того, как монитор начнёт слать события из
        // задач опроса/UART; иначе emitEvent ругался бы «unregistered event».
        _eventSocket->registerEvent("alarm");
    }
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

    size_t subscriberCount = 0;
    bool added = false;
    {
        MonitorLock lock(_lock);
        // Проверяем, не подписан ли уже
        const bool already_subscribed = std::find(_subscribers.begin(), _subscribers.end(), subscriber) != _subscribers.end();

        if (!already_subscribed) {
            _subscribers.push_back(subscriber);
            added = true;
        }
        subscriberCount = _subscribers.size();
    }

    if (added) {
        ESP_LOGD(TAG, "Subscriber 0x%p successfully added. Total subscribers: %zu.",
                 static_cast<void*>(subscriber), subscriberCount);
    } else {
        ESP_LOGD(TAG, "Subscriber 0x%p is already subscribed. Skipping.", static_cast<void*>(subscriber));
    }
}

// Файл: AlarmMonitor.cpp

// Добавьте этот метод, который будет вызываться при изменении настроек
void AlarmMonitor::onThresholdsUpdated(const String& originId) {
    ESP_LOGI(TAG, "Threshold settings changed (Origin: %s). Re-checking all sensors immediately.", originId.c_str());

    std::vector<IAlarmSubscriber*> subscribers;
    {
        MonitorLock lock(_lock);
        subscribers = _subscribers;
    }
    for (IAlarmSubscriber* subscriber : subscribers) {
        subscriber->forceResetAlarm(); // <-- ВЫЗЫВАЕМ НОВЫЙ МЕТОД
    }
    // Обнуляем предыдущие состояния, чтобы гарантировать, что НОВЫЙ уровень сработает
    // даже если он совпадает со старым, но был пропущен из-за таймаута.
    // Если этого не сделать, в _last_alarm_states могут остаться старые уровни,
    // и новый уровень (например, CRITICAL) не сработает, если он уже был CRITICAL
    // до изменения порогов.
    {
        MonitorLock lock(_lock);
        _last_alarm_states.clear();
    }

    // Принудительно запускаем проверку
    checkAllSensors();
}


void AlarmMonitor::unsubscribe(IAlarmSubscriber* subscriber) {
    if (subscriber == nullptr) {
        ESP_LOGW(TAG, "Attempted to unsubscribe a NULL pointer.");
        return;
    }

    size_t old_size = 0;
    size_t new_size = 0;
    {
        MonitorLock lock(_lock);
        old_size = _subscribers.size();
        // Используем erase-remove idiom для удаления указателя из вектора
        _subscribers.erase(
            std::remove(_subscribers.begin(), _subscribers.end(), subscriber),
            _subscribers.end()
        );
        new_size = _subscribers.size();
    }

    // Логируем результат отписки
    if (new_size < old_size) {
        ESP_LOGI(TAG, "Subscriber 0x%p successfully removed. Total subscribers: %zu.",
                 static_cast<void*>(subscriber), new_size);
    } else {
        ESP_LOGD(TAG, "Subscriber 0x%p was not found in the list.", static_cast<void*>(subscriber));
    }
}


void AlarmMonitor::notifySubscribers(const AlarmEvent& event)
{
    std::vector<IAlarmSubscriber*> subscribers;
    {
        MonitorLock lock(_lock);
        subscribers = _subscribers;
    }
    if (event.source_kind == AlarmSourceKind::HARDWARE_FAULT) {
        ESP_LOGW(TAG,
                 "--- HARDWARE FAULT --- role=%s addr=0x%02X code=%u level=%d. Notifying %zu subscribers.",
                 event.hw_device_role ? event.hw_device_role : "?",
                 static_cast<unsigned>(event.hw_i2c_address),
                 static_cast<unsigned>(event.hw_code), static_cast<int>(event.level),
                 subscribers.size());
    } else {
        if (event.sensor == nullptr) {
            ESP_LOGE(TAG, "SENSOR alarm event with null sensor");
            return;
        }
        ESP_LOGW(TAG, "--- ALARM EVENT TRIGGERED --- Address: %s, Value: %.2f, Level: %d. Notifying %zu subscribers.",
                 SensorManager::addressToString(event.sensor->getAddress()).c_str(),
                 event.current_value,
                 static_cast<int>(event.level),
                 subscribers.size());
    }

    // В /ws/events отправляем до обхода подписчиков: часть из них делает
    // сетевые вызовы (Telegram/VK), и поток событий не должен их ждать.
    // Для hardware этот вызов обычно уже сделан из raise/clear и будет
    // подавлен таблицей последних отправленных состояний.
    publishAlarmEvent(event);

    for (IAlarmSubscriber* subscriber : subscribers) {
        if (subscriber) {
            if (event.source_kind == AlarmSourceKind::SENSOR && event.sensor != nullptr) {
                ESP_LOGV(TAG, "Notifying subscriber 0x%p about event for sensor %s.",
                         static_cast<void*>(subscriber),
                         SensorManager::addressToString(event.sensor->getAddress()).c_str());
            }
            subscriber->onAlarm(event);
        }
    }
}

bool AlarmMonitor::rememberAlarmEventState(const char* key, const uint8_t state)
{
    if (key == nullptr || key[0] == '\0') {
        return false;
    }

    MonitorLock lock(_lock);
    for (size_t i = 0; i < _lastEmittedAlarmCount; ++i) {
        if (strncmp(_lastEmittedAlarmStates[i].key, key,
                    sizeof(_lastEmittedAlarmStates[i].key)) == 0) {
            if (_lastEmittedAlarmStates[i].state == state) {
                return false;  // то же состояние уже отправлено — повод молчать
            }
            _lastEmittedAlarmStates[i].state = state;
            return true;
        }
    }

    // Снятие без предшествующего подъёма не отправляем: иначе после старта
    // устройства клиенты получили бы пачку «пустых» снятий.
    if (state == 0) {
        return false;
    }

    size_t slot;
    if (_lastEmittedAlarmCount < kMaxLastEmittedAlarmStates) {
        slot = _lastEmittedAlarmCount++;
    } else {
        // Таблица заполнена — переиспользуем слот по кругу: память не растёт,
        // а повторно встреченный ключ снова получит запись при следующем
        // переходе.
        slot = _nextLastEmittedAlarmSlot;
        _nextLastEmittedAlarmSlot =
            (_nextLastEmittedAlarmSlot + 1) % kMaxLastEmittedAlarmStates;
    }
    strncpy(_lastEmittedAlarmStates[slot].key, key,
            sizeof(_lastEmittedAlarmStates[slot].key) - 1);
    _lastEmittedAlarmStates[slot].key[sizeof(_lastEmittedAlarmStates[slot].key) - 1] = '\0';
    _lastEmittedAlarmStates[slot].state = state;
    return true;
}

void AlarmMonitor::publishAlarmEvent(const AlarmEvent& event)
{
    if (_eventSocket == nullptr) {
        // Штатный режим «без сокета»: уведомления и REST работают как раньше.
        return;
    }

    char key[48];
    const bool hardware = event.source_kind == AlarmSourceKind::HARDWARE_FAULT;
    std::string address;
    std::string zone;
    const char* role = nullptr;
    const char* code = nullptr;
    uint8_t state = 0;

    if (hardware) {
        role = event.hw_device_role;
        code = event.hw_code == HardwareFaultCode::NONE
                   ? nullptr
                   : hardwareFaultCodeKey(event.hw_code);
        state = static_cast<uint8_t>(event.hw_code);
        snprintf(key, sizeof(key), "h:%s:0x%02X", role != nullptr ? role : "?",
                 static_cast<unsigned>(event.hw_i2c_address));
    } else {
        if (event.sensor == nullptr) {
            return;  // notifySubscribers уже сообщил об этом в лог
        }
        address = SensorManager::addressToString(event.sensor->getAddress());
        zone = SensorZoneHelper::toString(event.sensor->getZone());
        state = static_cast<uint8_t>(event.level);
        snprintf(key, sizeof(key), "s:%s", address.c_str());
    }

    // Только переход. Повторный опрос шины и повторная проверка после смены
    // порогов дают то же состояние — событие не дублируется.
    if (!rememberAlarmEventState(key, state)) {
        return;
    }

    // Подписчика без подключённого клиента быть не может, а emitEvent
    // сериализует кадр даже при пустом списке подписок. Состояние выше уже
    // записано, поэтому настоящий переход (clear → raise) не будет подавлен
    // позже из-за «устаревшей» записи.
    if (_eventSocket->getConnectedClients() == 0) {
        return;
    }

    const bool active = hardware ? event.hw_code != HardwareFaultCode::NONE
                                 : event.level != AlarmLevel::NORMAL;

    // Кадр ~150–200 Б — внутренняя куча, PSRAM ради него не нужна.
    JsonDocument document;
    JsonObject data = document.to<JsonObject>();
    data["kind"] = hardware ? "hardware" : "threshold";
    data["level"] = alarmLevelKey(event.level);
    if (address.empty()) {
        data["address"] = nullptr;  // у hardware адрес есть только числом в REST
    } else {
        data["address"] = address;
    }
    if (zone.empty()) {
        data["zone"] = nullptr;
    } else {
        data["zone"] = zone;
    }
    if (role == nullptr) {
        data["role"] = nullptr;
    } else {
        data["role"] = role;
    }
    if (code == nullptr) {
        data["code"] = nullptr;  // снятие: активного кода нет
    } else {
        data["code"] = code;
    }
    data["active"] = active;
    data["observed_at"] = millis();

    // Известное ограничение: вызов идёт из задач опроса/UART/веб-запроса, а не
    // из loop-задачи. Подписки в EventSocket защищены мьютексом, но
    // AsyncWebSocket::binary() из чужой задачи надо проверить на устройстве.
    _eventSocket->emitEvent("alarm", data);
}

void AlarmMonitor::checkAllSensors() {
    if (!_thresholdService) {
        ESP_LOGE(TAG, "Cannot check sensors: Threshold Service is NULL.");
        return;
    }

    ESP_LOGV(TAG, "Starting sensor check cycle.");

    // 1. Получаем доступ к текущим настройкам порогов
    _thresholdService->read([&](const AlarmThresholdsState& thresholdsState) {

        // 2. Получаем все датчики
        const SensorManager& sm = SensorManager::getInstance();
        const auto& all_sensors = sm.getAllSensors();
        ESP_LOGV(TAG, "Processing %zu sensors.", all_sensors.size());

        for (const auto& pair : all_sensors) {
            const std::string& addr_str = pair.first;
            AbstractSensor* sensor = pair.second;

            // 3. Проверяем, есть ли для этого датчика настройки
            if (thresholdsState.sensor_thresholds.count(addr_str) == 0) {
                ESP_LOGD(TAG, "Sensor %s skipped: No threshold settings found.", addr_str.c_str());
                continue; // Настроек нет, пропускаем
            }

            const ThresholdSettings& settings = thresholdsState.sensor_thresholds.at(addr_str);
            if (!settings.enabled) {
                ESP_LOGD(TAG, "Sensor %s skipped: Monitoring is disabled.", addr_str.c_str());
                continue; // Мониторинг для датчика выключен
            }
            // Подробный лог с настройками
            ESP_LOGV(TAG, "Sensor %s: Thresh (Crit:%.2f, Dang:%.2f, Min:%.2f).",
                     addr_str.c_str(), settings.critical_threshold, settings.dangerous_threshold, settings.min_threshold);
            // 4. Сравнение
            const float current_value = sensor->getData();


            // 4.1. Проверка валидности данных
         if (!sensor->isDataValid()) {

             // Защита от ложной тревоги на старте:
             // Если датчика еще нет в списке _last_alarm_states, это первый цикл.
             bool is_first_check = false;
             {
                 MonitorLock lock(_lock);
                 is_first_check = (_last_alarm_states.count(addr_str) == 0);
                 if (is_first_check) {
                     _last_alarm_states[addr_str] = AlarmLevel::NORMAL;
                 }
             }

             if (is_first_check) {
                 // Инициализируем состояние как NORMAL, но не генерируем событие.
                 ESP_LOGD(TAG, "Sensor %s is in initial state (non-valid data). Skipping alarm check.", addr_str.c_str());
                 continue; // Пропускаем, чтобы избежать ложной тревоги MINIMUM
             }

             // Если код дошел сюда, это СБОЙ ВО ВРЕМЯ РАБОТЫ.
             auto new_level = AlarmLevel::CRITICAL;
             AlarmLevel last_level = AlarmLevel::NORMAL;
             bool shouldNotify = false;
             {
                 MonitorLock lock(_lock);
                 last_level = _last_alarm_states.count(addr_str) ?
                              _last_alarm_states.at(addr_str) :
                              AlarmLevel::NORMAL;
                 if (new_level != last_level) {
                     _last_alarm_states[addr_str] = new_level;
                     shouldNotify = true;
                 }
             }

             if (shouldNotify) {
                 ESP_LOGE(TAG, "SENSOR FAILURE DETECTED! Sensor %s transition: %d -> %d. Value: %.2f (Non-valid data)",
                          addr_str.c_str(), static_cast<int>(last_level), static_cast<int>(new_level), current_value);

                 AlarmEvent event{};
                 event.source_kind = AlarmSourceKind::SENSOR;
                 event.sensor = sensor;
                 event.current_value = current_value;
                 event.threshold_value = FAILURE_THRESHOLD_VALUE;
                 event.level = new_level;
                 event.timestamp = time(nullptr);
                 notifySubscribers(event);
             }
             continue; // Завершаем обработку
         }

            auto new_level = AlarmLevel::NORMAL;
            float threshold_crossed = 0.0f;

            // Логируем текущее значение
            ESP_LOGV(TAG, "Sensor %s read value: %.2f.", addr_str.c_str(), current_value);

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
            AlarmLevel last_level = AlarmLevel::NORMAL; // Предполагаем NORMAL, если не было предыдущего состояния
            bool shouldNotify = false;
            {
                MonitorLock lock(_lock);
                last_level = _last_alarm_states.count(addr_str) ?
                             _last_alarm_states.at(addr_str) :
                             AlarmLevel::NORMAL;
                if (new_level != last_level) {
                    _last_alarm_states[addr_str] = new_level; // Обновляем состояние
                    shouldNotify = true;
                }
            }

            // Логируем переход состояния
            if (shouldNotify) {
                ESP_LOGI(TAG, "STATUS CHANGE! Sensor %s transition: %d -> %d. Value: %.2f (Crossed: %.2f)",
                         addr_str.c_str(), static_cast<int>(last_level), static_cast<int>(new_level), current_value, threshold_crossed);

                AlarmEvent event{};
                event.source_kind = AlarmSourceKind::SENSOR;
                event.sensor = sensor;
                event.current_value = current_value;
                event.threshold_value = threshold_crossed;
                event.level = new_level;
                event.timestamp = time(nullptr);
                notifySubscribers(event);

            } else {
                // Подробный лог, если состояние не изменилось
                ESP_LOGV(TAG, "Sensor %s: State remains %d (Value: %.2f). No notification sent.",
                         addr_str.c_str(), static_cast<int>(new_level), current_value);
            }
        }
    });

    ESP_LOGV(TAG, "Sensor check cycle finished.");
}