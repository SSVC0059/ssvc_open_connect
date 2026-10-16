#ifndef XKC25_SENSOR_H
#define XKC25_SENSOR_H

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

#include "components/sensors/AbstractSensor/AbstractSensor.h"
#include "components/sensors/PollingSubsystem/PollingSubsystem.h"
#include "components/io/DigitalInput/DigitalInputPin.h"
#include "core/StatefulServices/OpenConnectHardwareSettingsService/OpenConnectHardwareConfig.h"

class Xkc25Sensor;

/**
 * @brief Наблюдатель за изменением дискретного уровня XKC-25.
 *
 * Событие приходит только на смене уровня (фронт/спад), а не на каждом опросе,
 * поэтому потребитель может выполнять дорогие операции (UART-команда, push)
 * без риска задублировать их на каждом цикле.
 */
class IXkc25LevelObserver {
public:
    virtual ~IXkc25LevelObserver() = default;

    /** @param filled true — датчик сработал (ёмкость заполнена); false — сигнал снят. */
    virtual void onXkc25LevelChanged(Xkc25Sensor& sensor, bool filled) = 0;
};

/**
 * @brief Дискретный датчик уровня XKC-25-NPN (бесконтактный ёмкостный, выход открытого коллектора).
 *
 * Датчик крепится на внешнюю стенку неметаллической ёмкости и реагирует на
 * жидкость через стенку. Каждый датчик сидит на собственном GPIO.
 * getData() возвращает 0.0 (жидкости напротив датчика нет) или 1.0 (жидкость есть).
 * Зона задаётся полем zoneIndex: 0/1/2 → RECEIVER_TANK_1/2/3. Номер пина хранится в address[0].
 */
class Xkc25Sensor final : public AbstractSensor, public PollingSubsystem {
public:
    /**
     * @param config настройки датчика из OpenConnectHardwareConfig.
     * @param index  индекс датчика в конфигурации (0..2); задаёт номер зоны и имя.
     */
    Xkc25Sensor(const Xkc25SensorConfig& config, uint8_t index);

    // PollingSubsystem
    const std::string& getName() const override;
    bool initialize() override;
    void poll() override;

    // AbstractSensor
    void readValue() override;
    float getData() const override;
    std::string getUnit() const override;
    std::string getType() const override;
    MeasuredValueType getMeasurementType() const override;

    /** Зона приёмной ёмкости по zoneIndex из конфигурации (0..2): RECEIVER_TANK_1/2/3. */
    static SensorZone zoneFromIndex(uint8_t zoneIndex);

    uint8_t getGpio() const { return _src.gpio; }
    uint8_t getIndex() const { return _index; }
    bool isActiveLow() const { return _activeLow; }

    /** Источник сигнала: 0 — нативный GPIO, 1 — бит входного расширителя PCF8574. */
    uint8_t getSource() const { return _src.source; }
    uint8_t getPcfAddress() const { return _src.pcfAddress; }
    uint8_t getPcfBit() const { return _src.pcfBit; }
    bool isPcfSource() const { return _src.isPcf(); }

    /** Человекочитаемая подпись источника: «GPIO 4» / «PCF8574 0x22 · P3». */
    std::string pinLabel() const { return _src.label(); }

    /** Индекс приёмной ёмкости (0..2), за которой датчик закреплён сейчас. */
    uint8_t getZoneIndex() const { return _zoneIndex; }

    /** true — датчик переносной: один прибор используется на всех приёмных ёмкостях. */
    bool isSharedAcrossTanks() const { return _sharedAcrossTanks; }

    /**
     * @brief Переключает зону датчика без пересоздания объекта.
     *
     * Нужно переносному датчику: прибор физически переставляется с ёмкости на
     * ёмкость, а зона в SensorManager следует за настройкой `xkc25SharedTankIndex`.
     */
    void setZoneIndex(uint8_t zoneIndex);

    /** Текущее состояние: true — ёмкость заполнена (сигнал активен). */
    bool isFilled() const { return _lastValue >= 0.5f; }

    /** Сырой уровень пина без инверсии (0/1); нужен калибровке и статусу. */
    int readRawPinLevel() const;

    /**
     * @brief Применяет результат калибровки: полярность и режим пина.
     *
     * Текущее состояние пересчитывается без генерации события фронта —
     * калибровка не является срабатыванием датчика.
     */
    void applyCalibration(bool activeLow);

    /**
     * @brief Серия чтений пина с проверкой стабильности сигнала.
     * @param samples    число чтений
     * @param intervalMs пауза между чтениями
     * @param levelOut   итоговый сырой уровень; валиден только при возврате true
     * @return true — все чтения дали один и тот же уровень; false — сигнал нестабилен.
     */
    bool sampleStable(uint8_t samples, uint16_t intervalMs, int& levelOut) const;

    /**
     * @brief Подписывает получателя событий фронта/спада.
     * @param observer указывает на долгоживущий объект (подсистема); nullptr — отписка.
     */
    void setLevelObserver(IXkc25LevelObserver* observer) { _observer = observer; }

    /**
     * @brief Запись в журнал сырого уровня пина и результата расчёта.
     *
     * Пишет строку не чаще kRawLogIntervalMs: сырое чтение (0/1), режим пина,
     * применённая полярность и итог «ёмкость заполнена / не заполнена».
     */
    void logRawState();

    /** Результат пробы линии: уровни при обеих внутренних подтяжках и вердикт. */
    struct LineProbe {
        int pullUp = -1;
        int pullDown = -1;
        std::string verdict;

        /** Проба выполнена: оба уровня сняты. */
        bool valid() const { return pullUp >= 0 && pullDown >= 0; }
    };

    /**
     * @brief Проба линии: сырые чтения при внутренней подтяжке вверх и вниз.
     *
     * Полярность на момент калибровки ещё не определена, поэтому одного рабочего
     * режима мало: сравнивая обе подтяжки в покое и при срабатывании, видно, в
     * каком из режимов состояния датчика различимы, — это и даёт полярность.
     * Заодно проба отвечает, почему уровень пина не меняется: линия прижата
     * извне (к питанию или к земле) либо высокоомна и следует за подтяжкой —
     * обрыв, не тот пин, выход датчика в третьем состоянии.
     *
     * Рабочий режим пина восстанавливается до выхода.
     *
     * @param stage метка состояния («без жидкости» / «с жидкостью») для журнала.
     */
    LineProbe probeLine(const char* stage) const;

private:
    static Address& createAddr(const Xkc25SensorConfig& config);
    static std::string makeName(uint8_t index);

    /** Уровень по большинству из трёх чтений: одиночное может попасть на перезаряд линии. */
    int readLevelMajority() const;

    /**
     * @brief Приводит пин к рабочему режиму по текущей полярности.
     *
     * GPIO: pinMode(INPUT) — подтяжку делает монтаж. PCF8574: подготовка входов
     * расширителя (write 0xFF). Идемпотентно; вызывается из initialize() и после
     * калибровки.
     */
    void applyInputMode() const;

    /** Минимальный интервал между строками журнала с сырым уровнем. */
    static constexpr uint32_t kRawLogIntervalMs = 5000;

    uint8_t _index;
    DigitalInputSource _src;
    /** begin()/read() вызываются и из const-методов (проба линии, калибровка). */
    mutable DigitalInputPin _pin;
    uint8_t _zoneIndex;
    bool _activeLow;
    /** Один прибор на все ёмкости: зону задаёт общая настройка, а не конфиг слота. */
    bool _sharedAcrossTanks;
    float _lastValue = 0.0f;
    /** Время последней записи сырого уровня в журнал (0 — ещё не писали). */
    uint32_t _lastRawLogMs = 0;
    IXkc25LevelObserver* _observer = nullptr;
};

#endif // XKC25_SENSOR_H
