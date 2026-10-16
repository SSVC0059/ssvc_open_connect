#pragma once

#include "core/SubsystemManager/SubsystemManager.h"
#include "components/sensors/Xkc25Sensor/Xkc25Sensor.h"
#include "components/sensors/PollingSubsystem/PollingSubsystem.h"

#include <array>
#include <functional>
#include <string>
#include <vector>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

/**
 * Машинные коды итога шага калибровки (поле Xkc25CalibrationResult::code).
 * API сопоставляет код с HTTP-статусом ответа, UI показывает message.
 */
namespace Xkc25CalibrationCode {
constexpr const char* OK = "ok";
constexpr const char* SENSOR_NOT_AVAILABLE = "sensor_not_available";
constexpr const char* SENSOR_NOT_CONFIGURED = "sensor_not_configured";
constexpr const char* UNSTABLE_SIGNAL = "unstable_signal";
constexpr const char* NO_EXTERNAL_PULL = "no_external_pull";
constexpr const char* NOT_REACTED = "not_reacted";
constexpr const char* NOT_CALIBRATED = "not_calibrated";
constexpr const char* PERSIST_FAILED = "persist_failed";
constexpr const char* VERIFY_FAILED = "verify_failed";
} // namespace Xkc25CalibrationCode

/** Итог одного шага мастера калибровки XKC-25. */
struct Xkc25CalibrationResult {
    bool ok = false;                    // шаг выполнен (уровень снят и, где нужно, сохранён)
    bool completed = false;             // калибровка целиком завершена (уровни различаются)
    int rawLevel = -1;                  // снятый сырой уровень пина; -1 — чтение не выполнялось
    const char* code = Xkc25CalibrationCode::OK; // машинный код итога для API
    std::string message;                // пояснение для UI
    std::vector<std::string> checklist; // чек-лист подключения при отклонённой калибровке
};

/** Итог предстартовой проверки XKC-25 (режим xkc25StartGuardMode). */
struct Xkc25StartCheck {
    bool allow = true;                     // false — старт отклоняется (режим block)
    uint8_t mode = 1;                      // Xkc25StartGuardMode, действовавший в момент проверки
    std::vector<std::string> blockReasons; // причины отказа по каждому датчику
    std::vector<std::string> warnings;     // предупреждения (режим warn, уже заполненные ёмкости)
};

/**
 * @brief Подсистема дискретных датчиков уровня XKC-25-NPN.
 *
 * Читает массив `xkc25Sensors` из OpenConnectHardwareConfig и создаёт по одному
 * Xkc25Sensor на каждый включённый элемент (0..3), регистрируя их в SensorManager
 * и SensorCoordinator. При отсутствии включённых датчиков отключает себя.
 *
 * События фронта/спада получает от датчиков как IXkc25LevelObserver и агрегирует их:
 * пока сработал хотя бы один датчик, отбор держится на паузе, поэтому снятие
 * отдельного датчика не возобновляет процесс.
 *
 * Калибровка выполняется по шагам («без жидкости» → «с жидкостью» → проверка возврата):
 * датчик бесконтактный, поэтому фиксируются сырые уровни пина в двух состояниях
 * жидкости, из них вычисляется полярность, а совпадение уровней означает, что датчик
 * не отреагировал и калибровка отклоняется.
 */
class Xkc25Subsystem final : public Subsystem, public IXkc25LevelObserver {
public:
    Xkc25Subsystem() = default;

    ~Xkc25Subsystem() override = default;

    static std::string getName() { return "xkc25_sensor"; }

    void initialize() override;
    void enable() override;
    void disable() override;

    // IXkc25LevelObserver
    void onXkc25LevelChanged(Xkc25Sensor& sensor, bool filled) override;

    // --- Мастер калибровки (6.2) ---

    /** Шаг «без жидкости»: фиксирует сырой уровень пина, когда жидкости напротив датчика нет. */
    Xkc25CalibrationResult recordLevelWithoutLiquid(uint8_t sensorIndex);

    /** Шаг «с жидкостью»: фиксирует уровень с жидкостью и завершает калибровку при различии уровней. */
    Xkc25CalibrationResult recordLevelWithLiquid(uint8_t sensorIndex);

    /** Шаг «проверка возврата»: ждёт возврата пина к levelWithoutLiquid после опускания уровня. */
    Xkc25CalibrationResult verifyReturn(uint8_t sensorIndex, uint32_t timeoutMs);

    // --- Предстартовая проверка (6.4) ---

    /**
     * @brief Проверка перед стартом ректификации.
     *
     * Статическая: защита обязана работать и когда подсистема вообще не создана
     * (нет ни одного включённого датчика — тогда проверять нечего и старт разрешён).
     */
    static Xkc25StartCheck checkStart();

    // --- Доступ к состоянию датчиков для API (фаза 7) ---

    /** Живая подсистема; nullptr — ни один датчик не поднят (нет включённых или нет перезагрузки). */
    static Xkc25Subsystem* activeInstance() { return _active; }

    /** Датчик по индексу слота; nullptr — датчик выключен или не поднялся. */
    Xkc25Sensor* sensorByIndex(uint8_t index) const { return findSensor(index); }

    /** Ключ зоны приёмной ёмкости по zoneIndex (0..2): receiver_tank_1/2/3. */
    static std::string zoneKey(uint8_t zoneIndex);

    /** Человекочитаемое имя зоны приёмной ёмкости по zoneIndex (0..2). */
    static std::string zoneLabel(uint8_t zoneIndex);

private:
    /**
     * @brief Опрос состояния процесса для уведомления о старте в обход ESP32 (6.5).
     *
     * Xkc25Subsystem уже владеет датчиками и не может быть PollingSubsystem сам
     * (у него статический getName() для SubsystemManager), поэтому цикл опроса
     * получает этот маленький адаптер.
     */
    class ProcessWatcher final : public PollingSubsystem {
    public:
        explicit ProcessWatcher(Xkc25Subsystem& owner) : _owner(owner) {}
        bool initialize() override { return true; }
        void poll() override { _owner.checkProcessTransition(); }
        const std::string& getName() const override { return _name; }

    private:
        Xkc25Subsystem& _owner;
        std::string _name = "xkc25_process_watcher";
    };

    /** Отслеживает переход процесса в RUNNING и уведомляет о старте без калибровки. */
    void checkProcessTransition();

    /**
     * @brief Быстрый опрос входов XKC-25 (отдельная задача, ~100 мс).
     *
     * Централизованный опрос SensorCoordinator идёт раз в 10 с — для датчика уровня
     * это слишком медленно (реакция должна быть практически мгновенной). Поэтому
     * датчики опрашивает собственная задача подсистемы; в SensorCoordinator они не
     * регистрируются. Пороги алертов всё равно проверяются в цикле координатора
     * (AlarmMonitor обходит SensorManager), данные там уже свежие.
     */
    static void fastPollTaskEntry(void* arg);
    void fastPollLoop();

    /** Проверка целостности конфигурации после загрузки (6.3); пишет файл только при изменении. */
    void runBootAutocheck();

    /**
     * @brief Применяет текущую ёмкость переносного датчика из `xkc25SharedTankIndex`.
     *
     * Датчик один и переставляется с ёмкости на ёмкость: пользователь меняет настройку
     * в UI, а подсистема переводит зону датчика (и его запись в SensorManager) на неё.
     * Калибровка при этом не затрагивается.
     */
    void syncSharedTankZone();

    Xkc25Sensor* findSensor(uint8_t index) const;

    /** Есть ли включённый датчик без калибровки. */
    static bool hasUncalibratedEnabledSensor();

    /** Отправляет предупреждение в UI (push-уведомление + лог). */
    static void pushWarning(const std::string& message);

    /** Отправляет push-уведомление «ёмкость заполнена» в UI. */
    void notifyTankFilled(const Xkc25Sensor& sensor) const;

    /** Сохраняет изменение одного датчика через OpenConnectHardwareSettingsService. */
    static bool persistSensor(uint8_t index, const std::function<void(Xkc25SensorConfig&)>& mutate);

    /** Итог: пауза принадлежит датчикам и её нужно снять, когда все они отпустят сигнал. */
    bool pauseOwnedBySensors() const { return _sensorsOnAlert > 0 ? false : _pauseIssuedByXkc; }

    std::vector<Xkc25Sensor*> _sensors;
    ProcessWatcher _watcher{*this};
    /** Задача быстрого опроса входов; nullptr — не запущена. */
    TaskHandle_t _fastPollTask = nullptr;
    /** Сигнал задаче быстрого опроса завершиться при disable(). */
    volatile bool _fastPollStop = false;
    /**
     * Проба линии на шаге «без жидкости» (индекс датчика → уровни при обеих
     * подтяжках и вердикт). Нужна на шаге «с жидкостью»: сравнение проб в двух
     * состояниях даёт полярность, а вердикт объясняет неподвижный уровень пина.
     */
    std::array<Xkc25Sensor::LineProbe, OpenConnectHardwareConfig::kMaxXkc25Sensors> _withoutLiquid{};
    /** Сколько датчиков прямо сейчас сигнализирует о заполнении. */
    size_t _sensorsOnAlert = 0;
    /** Пауза выставлена этим счётчиком, значит снимаем её сами при спаде. */
    bool _pauseIssuedByXkc = false;
    bool _initialized = false;
    bool _enabled = false;
    /** Состояние процесса на прошлом опросе: уведомляем только на переходе в RUNNING. */
    bool _processWasRunning = false;

    /** Живая подсистема: предстартовой проверке нужны текущие состояния датчиков. */
    static Xkc25Subsystem* _active;

    static constexpr uint8_t kCalibrationSamples = 5;
    static constexpr uint16_t kCalibrationSampleIntervalMs = 20;
    static constexpr uint32_t kVerifyReturnDefaultTimeoutMs = 5000;
    static constexpr uint16_t kVerifyReturnPollMs = 100;
    /** Период быстрого опроса входов: 100 мс × дебаунс 3 ≈ 300 мс до реакции. */
    static constexpr uint32_t kFastPollIntervalMs = 100;
    static constexpr uint32_t kFastPollStackWords = 4096;

    static constexpr auto TAG = "Xkc25Subsystem";
};
