#include "Xkc25Subsystem.h"

#include "core/SubsystemManager/SubsystemManager.h"
#include "core/StatefulServices/OpenConnectHardwareSettingsService/OpenConnectHardwareConfig.h"
#include "core/StatefulServices/OpenConnectHardwareSettingsService/OpenConnectHardwareSettingsService.h"
#include "components/sensors/SensorManager/SensorManager.h"
#include "components/sensors/SensorCoordinator/SensorCoordinator.h"
#include "core/SsvcCommandsQueue.h"
#include "core/SsvcOpenConnect.h"
#include "core/rectification/RectificationProcess.h"

#include <Arduino.h>
#include <cstdio>
#include <ctime>
#include <esp_log.h>

Xkc25Subsystem* Xkc25Subsystem::_active = nullptr;

namespace {

/** UART-команда осмысленна только когда процесс уже шёл: IDLE/FINISHED/STOPPED не трогаем. */
bool isProcessControllable(const RectificationTypes::ProcessState state) {
    return state == RectificationTypes::ProcessState::RUNNING ||
           state == RectificationTypes::ProcessState::PAUSED;
}

/** Человекочитаемая подпись входа датчика по его конфигу («GPIO 4» / «PCF8574 0x22 · P3»). */
std::string sensorInputLabel(const Xkc25SensorConfig& sensor) {
    DigitalInputSource src;
    src.source = sensor.pinSource;
    src.gpio = sensor.gpio;
    src.pcfAddress = sensor.pcfAddress;
    src.pcfBit = sensor.pcfBit;
    return src.label();
}

/**
 * Чек-лист подключения бесконтактного датчика: показывается, когда уровни совпали
 * и датчик не подтвердил реакцию на жидкость. Пункты зависят от источника сигнала:
 * у нативного GPIO подтяжку линии делает внешний резистор, у входного PCF8574 — сам
 * расширитель.
 */
std::vector<std::string> calibrationChecklist(const bool pcfSource) {
    if (pcfSource) {
        return {
            "Питание датчика DC 5–12 В (коричневый провод) и GND (синий)",
            "Выход OUT (жёлтый провод) — на входном расширителе PCF8574 (адрес 0x22, сухой контакт)",
            "Выход открытого коллектора (NPN): вход расширителя уже подтянут к 3.3 В, "
            "внешний резистор не нужен",
            "Адрес входного расширителя не занят выходами реле",
            "Неметаллическая стенка ёмкости, зазор/толщина стенки не более 20 мм",
            "Чувствительность датчика (винт на корпусе)",
            "Выбранный бит расширителя соответствует фактическому входу PCF8574",
            "Для PNP-версии (высокий уровень = напряжение питания) нужен внешний "
            "транзистор/делитель: подавать 5–12 В на вход расширителя нельзя",
        };
    }
    return {
        "Питание датчика DC 5–12 В (коричневый провод)",
        "Три провода: коричневый — VCC, жёлтый — OUT на выбранном GPIO, синий — GND",
        "Неметаллическая стенка ёмкости, зазор/толщина стенки не более 20 мм",
        "Чувствительность датчика (винт на корпусе)",
        "Выбранный GPIO соответствует фактическому подключению выхода OUT",
        "Внешняя подтяжка линии резистором: к земле для высокоуровневого выхода (PNP), "
        "к 3.3 В для открытого коллектора (NPN) — прошивка линию не тянет",
        "Согласование уровня выхода с 3.3 В, если выход выдаёт напряжение питания "
        "датчика (5–12 В): делитель/транзистор/оптопара",
        "Тип выхода: замыкает на землю (NPN, открытый коллектор) или выдаёт высокий "
        "уровень (PNP) — полярность определит калибровка",
    };
}

} // namespace

std::string Xkc25Subsystem::zoneKey(const uint8_t zoneIndex) {
    return SensorZoneHelper::toString(Xkc25Sensor::zoneFromIndex(zoneIndex));
}

std::string Xkc25Subsystem::zoneLabel(const uint8_t zoneIndex) {
    return "Приёмная ёмкость " + std::to_string(static_cast<unsigned>(zoneIndex) + 1);
}

void Xkc25Subsystem::initialize() {
    _initialized = true;
    ESP_LOGI(TAG, "XKC-25 subsystem initialized");
}

void Xkc25Subsystem::enable() {
    if (!_initialized || _enabled) return;

    // Автопроверка целостности конфигурации до создания датчиков: исправленный
    // конфиг сразу попадает в работу (6.3).
    runBootAutocheck();

    std::vector<Xkc25SensorConfig> configs;
    uint8_t sharedTankIndex = 0;
    OpenConnectHardwareSettingsService::instance().read([&](OpenConnectHardwareConfig& cfg) {
        configs = cfg.xkc25Sensors;
        sharedTankIndex = cfg.xkc25SharedTankIndex;
    });

    if (configs.size() > OpenConnectHardwareConfig::kMaxXkc25Sensors) {
        configs.resize(OpenConnectHardwareConfig::kMaxXkc25Sensors);
    }

    for (size_t i = 0; i < configs.size(); ++i) {
        if (!configs[i].enabled) {
            continue;
        }

        // Переносной датчик один на все ёмкости: его зону задаёт не слот конфига,
        // а текущая настройка «на какой ёмкости датчик стоит сейчас» (req 3).
        if (configs[i].sharedAcrossTanks) {
            configs[i].zoneIndex = sharedTankIndex;
        }

        auto* sensor = new Xkc25Sensor(configs[i], static_cast<uint8_t>(i));

        // Two entries sharing a pin would collide in SensorManager (which deletes duplicates);
        // skip explicitly to avoid holding a pointer to a freed object.
        const std::string key = SensorManager::addressToString(sensor->getAddress());
        if (SensorManager::getInstance().isSensorRegistered(key)) {
            ESP_LOGW(TAG, "[%s] %s already in use — skipping", sensor->getName().c_str(),
                     sensor->pinLabel().c_str());
            delete sensor;
            continue;
        }

        // Подписка до initialize(): первое чтение задаёт исходное состояние и
        // событием не считается, но порядок оставляем единым на всё время жизни.
        sensor->setLevelObserver(this);

        if (!sensor->initialize()) {
            ESP_LOGE(TAG, "[%s] initialization failed — skipping", sensor->getName().c_str());
            sensor->setLevelObserver(nullptr);
            delete sensor;
            continue;
        }

        SensorManager::getInstance().registerSensor(sensor);
        SensorCoordinator::getInstance().registerPollingSubsystem(sensor);
        _sensors.push_back(sensor);

        // Текущее состояние после включения: без изменения уровня жидкости рабочий и
        // обесточенный датчик неразличимы, поэтому снимаем только фактическое чтение (6.3.1).
        ESP_LOGI(TAG, "[%s] Initial state: %s (%s, raw %d, calibrated=%d)",
                 sensor->getName().c_str(), sensor->isFilled() ? "liquid present" : "no liquid",
                 sensor->pinLabel().c_str(), sensor->readRawPinLevel(),
                 configs[i].calibrated ? 1 : 0);
    }

    if (_sensors.empty()) {
        ESP_LOGW(TAG, "No XKC-25 sensor enabled. Disabling subsystem.");
        SubsystemManager::instance().disableSubsystem(getName());
        return;
    }

    _active = this;
    _processWasRunning = false;
    // Опрос состояния процесса нужен для уведомления о старте в обход ESP32 (6.5).
    SensorCoordinator::getInstance().registerPollingSubsystem(&_watcher);

    _enabled = true;
    ESP_LOGI(TAG, "XKC-25 subsystem started with %zu sensor(s)", _sensors.size());
}

void Xkc25Subsystem::disable() {
    if (!_enabled) return;
    _enabled = false;
    _sensorsOnAlert = 0;
    _pauseIssuedByXkc = false;
    _processWasRunning = false;
    _active = nullptr;
    ESP_LOGI(TAG, "XKC-25 subsystem disabled");
}

void Xkc25Subsystem::onXkc25LevelChanged(Xkc25Sensor& sensor, const bool filled) {
    // Реакция — один взаимоисключающий режим: «пауза» и «остановка» одновременно недостижимы.
    uint8_t triggerAction = OpenConnectHardwareConfig::kXkc25ActionPause;
    OpenConnectHardwareSettingsService::instance().read([&](OpenConnectHardwareConfig& cfg) {
        triggerAction = cfg.xkc25TriggerAction;
    });

    RectificationProcess::Snapshot snapshot;
    const bool hasProcessState = RectificationProcess::rectController().getSnapshot(snapshot);

    if (filled) {
        ++_sensorsOnAlert;
        notifyTankFilled(sensor);

        if (!hasProcessState) {
            ESP_LOGW(TAG, "[%s] Process state unavailable — no UART command sent",
                     sensor.getName().c_str());
            return;
        }

        if (triggerAction == OpenConnectHardwareConfig::kXkc25ActionAbort) {
            if (isProcessControllable(snapshot.state)) {
                ESP_LOGW(TAG, "[%s] Tank filled — aborting rectification", sensor.getName().c_str());
                SsvcCommandsQueue::getQueue().stop();
            }
            return;
        }

        // pause() имеет смысл только из RUNNING: из PAUSED процесс уже удержан,
        // и тогда пауза не наша — снимать её на спаде мы не должны.
        if (triggerAction == OpenConnectHardwareConfig::kXkc25ActionPause &&
            snapshot.state == RectificationTypes::ProcessState::RUNNING) {
            ESP_LOGW(TAG, "[%s] Tank filled — pausing selection", sensor.getName().c_str());
            SsvcCommandsQueue::getQueue().pause();
            _pauseIssuedByXkc = true;
        }
        return;
    }

    if (_sensorsOnAlert > 0) {
        --_sensorsOnAlert;
    }

    ESP_LOGI(TAG, "[%s] Signal cleared (sensors on alert: %zu)", sensor.getName().c_str(),
             _sensorsOnAlert);

    if (!pauseOwnedBySensors()) {
        return;
    }

    if (hasProcessState && snapshot.state == RectificationTypes::ProcessState::PAUSED) {
        ESP_LOGI(TAG, "[%s] All level sensors clear — resuming selection", sensor.getName().c_str());
        SsvcCommandsQueue::getQueue().resume();
    }
    _pauseIssuedByXkc = false;
}

void Xkc25Subsystem::pushWarning(const std::string& message) {
    ESP_LOGW(TAG, "%s", message.c_str());

    ESP32SvelteKit* kit = SsvcOpenConnect::getInstance().getESP32SvelteKit();
    if (kit == nullptr || kit->getNotificationService() == nullptr) {
        ESP_LOGW(TAG, "NotificationService unavailable — push notification skipped");
        return;
    }
    kit->getNotificationService()->pushNotification(message.c_str(), PUSHWARNING);
}

void Xkc25Subsystem::notifyTankFilled(const Xkc25Sensor& sensor) const {
    // Переносному датчику важен не номер слота, а ёмкость, на которой он стоит сейчас.
    pushWarning(zoneLabel(sensor.getZoneIndex()) + " заполнена: жидкость напротив датчика (" +
                sensor.pinLabel() + ")");
}

Xkc25Sensor* Xkc25Subsystem::findSensor(const uint8_t index) const {
    for (Xkc25Sensor* sensor : _sensors) {
        if (sensor != nullptr && sensor->getIndex() == index) {
            return sensor;
        }
    }
    return nullptr;
}

bool Xkc25Subsystem::persistSensor(const uint8_t index,
                                   const std::function<void(Xkc25SensorConfig&)>& mutate) {
    bool found = false;
    bool dirty = false;
    const StateUpdateResult result = OpenConnectHardwareSettingsService::instance().update(
        [&](OpenConnectHardwareConfig& cfg) -> StateUpdateResult {
            if (index >= cfg.xkc25Sensors.size()) {
                return StateUpdateResult::UNCHANGED;
            }
            found = true;
            Xkc25SensorConfig& target = cfg.xkc25Sensors[index];
            const Xkc25SensorConfig before = target;
            mutate(target);
            dirty = !(Xkc25SensorConfig::structurallyEqual(before, target) &&
                      before.calibrated == target.calibrated &&
                      before.levelWithoutLiquid == target.levelWithoutLiquid &&
                      before.levelWithLiquid == target.levelWithLiquid &&
                      before.calibratedAt == target.calibratedAt);
            return dirty ? StateUpdateResult::CHANGED : StateUpdateResult::UNCHANGED;
        },
        "xkc25_calibration");

    ESP_LOGI(TAG, "Sensor #%u: calibration persist (found=%d, changed=%d, result=%d)",
             static_cast<unsigned>(index) + 1, found ? 1 : 0, dirty ? 1 : 0,
             static_cast<int>(result));

    // UNCHANGED — не сбой записи, а совпадение записываемого состояния с уже
    // сохранённым (частый случай: снятый уровень 1 равен дефолту levelWithoutLiquid).
    // Значение в файле уже такое, каким его хочет видеть калибровка, поэтому шаг
    // считается успешным; писать файл ради идентичных данных не нужно.
    return found;
}

Xkc25CalibrationResult Xkc25Subsystem::recordLevelWithoutLiquid(const uint8_t sensorIndex) {
    Xkc25CalibrationResult result;
    Xkc25Sensor* sensor = findSensor(sensorIndex);
    if (sensor == nullptr) {
        result.code = Xkc25CalibrationCode::SENSOR_NOT_AVAILABLE;
        result.message = "Датчик не найден или выключен";
        return result;
    }

    int raw = -1;
    if (!sensor->sampleStable(kCalibrationSamples, kCalibrationSampleIntervalMs, raw)) {
        ESP_LOGW(TAG, "Sensor #%u: step 'without liquid' rejected — unstable signal on GPIO %u",
                 static_cast<unsigned>(sensorIndex) + 1, static_cast<unsigned>(sensor->getGpio()));
        result.code = Xkc25CalibrationCode::UNSTABLE_SIGNAL;
        result.message = "Сигнал нестабилен: проверьте зазор до стенки и чувствительность датчика";
        return result;
    }
    result.rawLevel = raw;

    const uint8_t level = static_cast<uint8_t>(raw);
    ESP_LOGI(TAG, "Sensor #%u: step 'without liquid' — raw GPIO%u = %d, stable over %u samples",
             static_cast<unsigned>(sensorIndex) + 1, static_cast<unsigned>(sensor->getGpio()), raw,
             static_cast<unsigned>(kCalibrationSamples));

    // Проба линии в покое: её уровни нужны шагу «с жидкостью» для выбора
    // полярности, а вердикт — чтобы объяснить неподвижный уровень пина.
    const Xkc25Sensor::LineProbe idleProbe = sensor->probeLine("без жидкости");
    if (sensorIndex < _withoutLiquid.size()) {
        _withoutLiquid[sensorIndex] = idleProbe;
    }

    // Прошивка пин не подтягивает, поэтому уровень покоя обязан задавать внешний
    // резистор. Если линия следует за подтяжкой пробы, монтаж не готов: калибровка
    // прошла бы, а в работе чтения плавали бы на высокоомной линии.
    if (idleProbe.valid() && idleProbe.pullUp != idleProbe.pullDown) {
        ESP_LOGW(TAG, "Sensor #%u: idle line is not held by an external resistor (%s)",
                 static_cast<unsigned>(sensorIndex) + 1, idleProbe.verdict.c_str());
        result.code = Xkc25CalibrationCode::NO_EXTERNAL_PULL;
        result.message =
            "Линия не удерживается внешним резистором: в покое уровень задаёт только "
            "внутренняя подтяжка. Добавьте резистор — к земле для высокоуровневого выхода, "
            "к 3.3 В для открытого коллектора — и повторите калибровку";
        result.checklist = calibrationChecklist(sensor->isPcfSource());
        return result;
    }

    if (!persistSensor(sensorIndex, [level](Xkc25SensorConfig& cfg) {
            cfg.levelWithoutLiquid = level;
            // Калибровка не завершена, пока не снят второй уровень — с жидкостью.
            cfg.calibrated = false;
            cfg.calibratedAt = 0;
        })) {
        ESP_LOGE(TAG, "Sensor #%u: failed to record level without liquid", 
                 static_cast<unsigned>(sensorIndex) + 1);
        result.code = Xkc25CalibrationCode::PERSIST_FAILED;
        result.message = "Не удалось сохранить настройки датчика";
        return result;
    }

    result.ok = true;
    result.message = "Уровень без жидкости напротив датчика записан";
    return result;
}

Xkc25CalibrationResult Xkc25Subsystem::recordLevelWithLiquid(const uint8_t sensorIndex) {
    Xkc25CalibrationResult result;
    Xkc25Sensor* sensor = findSensor(sensorIndex);
    if (sensor == nullptr) {
        result.code = Xkc25CalibrationCode::SENSOR_NOT_AVAILABLE;
        result.message = "Датчик не найден или выключен";
        return result;
    }

    uint8_t levelWithoutLiquid = 0;
    bool found = false;
    OpenConnectHardwareSettingsService::instance().read([&](OpenConnectHardwareConfig& cfg) {
        if (sensorIndex < cfg.xkc25Sensors.size()) {
            levelWithoutLiquid = cfg.xkc25Sensors[sensorIndex].levelWithoutLiquid;
            found = true;
        }
    });
    if (!found) {
        result.code = Xkc25CalibrationCode::SENSOR_NOT_CONFIGURED;
        result.message = "Датчик не найден в конфигурации";
        return result;
    }

    int raw = -1;
    if (!sensor->sampleStable(kCalibrationSamples, kCalibrationSampleIntervalMs, raw)) {
        ESP_LOGW(TAG, "Sensor #%u: step 'with liquid' rejected — unstable signal on GPIO %u",
                 static_cast<unsigned>(sensorIndex) + 1, static_cast<unsigned>(sensor->getGpio()));
        result.code = Xkc25CalibrationCode::UNSTABLE_SIGNAL;
        result.message = "Сигнал нестабилен: проверьте зазор до стенки и чувствительность датчика";
        return result;
    }
    result.rawLevel = raw;

    const uint8_t level = static_cast<uint8_t>(raw);
    const Xkc25Sensor::LineProbe probeWithLiquid = sensor->probeLine("с жидкостью");
    const Xkc25Sensor::LineProbe noLiquidProbe =
        sensorIndex < _withoutLiquid.size() ? _withoutLiquid[sensorIndex] : Xkc25Sensor::LineProbe{};
    ESP_LOGI(TAG, "Sensor #%u: step 'with liquid' — raw GPIO%u = %d, without liquid = %u",
             static_cast<unsigned>(sensorIndex) + 1, static_cast<unsigned>(sensor->getGpio()), raw,
             static_cast<unsigned>(levelWithoutLiquid));

    // Полярность выбираем по пробам двух шагов: у высокоомной линии уровень задаёт
    // подтяжка, поэтому состояния различает тот режим, чьё чтение изменилось. Так
    // результат не зависит от того, что было записано до калибровки: у датчика с
    // активным высоким уровнем (PNP) покой подтяжкой вверх читается как
    // срабатывание, и без сравнения проб шаг «с жидкостью» видит «1 и 1».
    uint8_t levelWithoutLiquidOut = levelWithoutLiquid;
    uint8_t levelWithLiquid = level;
    if (noLiquidProbe.valid()) {
        if (probeWithLiquid.pullUp != noLiquidProbe.pullUp) {
            levelWithoutLiquidOut = static_cast<uint8_t>(noLiquidProbe.pullUp);
            levelWithLiquid = static_cast<uint8_t>(probeWithLiquid.pullUp);
        } else if (probeWithLiquid.pullDown != noLiquidProbe.pullDown) {
            levelWithoutLiquidOut = static_cast<uint8_t>(noLiquidProbe.pullDown);
            levelWithLiquid = static_cast<uint8_t>(probeWithLiquid.pullDown);
        }
    }
    result.rawLevel = levelWithLiquid;

    if (levelWithLiquid == levelWithoutLiquidOut) {
        // Датчик не подтвердил реакцию на жидкость: калибровка отклоняется,
        // в файл не пишется ничего (см. 6.2, шаг 3). Сырые данные обеих проб
        // идут в сообщение: по ним видно, что именно держит линию пина.
        std::string probeSummary = "без жидкости ";
        if (noLiquidProbe.valid()) {
            probeSummary += "вверх=" + std::to_string(noLiquidProbe.pullUp) + ", вниз=" +
                            std::to_string(noLiquidProbe.pullDown) + " (" +
                            noLiquidProbe.verdict + ")";
        } else {
            probeSummary += "проба не снята";
        }
        probeSummary += "; с жидкостью вверх=" + std::to_string(probeWithLiquid.pullUp) +
                        ", вниз=" + std::to_string(probeWithLiquid.pullDown) + " (" +
                        probeWithLiquid.verdict + ")";
        ESP_LOGW(TAG, "Sensor #%u: no reaction — level %u in both states; %s",
                 static_cast<unsigned>(sensorIndex) + 1, static_cast<unsigned>(levelWithLiquid),
                 probeSummary.c_str());
        result.code = Xkc25CalibrationCode::NOT_REACTED;
        result.message = "Датчик не отреагировал: линия не меняет состояние (" + probeSummary + ")";
        result.checklist = calibrationChecklist(sensor->isPcfSource());
        return result;
    }

    // Полярность — результат калибровки, а не константа «NPN ⇒ true»: она зависит
    // от того, куда подтянута линия (к 3.3 В или к питанию датчика).
    const bool activeLow = (levelWithLiquid == 0);
    const uint32_t calibratedAt = static_cast<uint32_t>(time(nullptr));
    ESP_LOGI(TAG,
             "Sensor #%u: calibration complete — activeLow=%d, without liquid=%u, with liquid=%u, "
             "calibratedAt=%lu",
             static_cast<unsigned>(sensorIndex) + 1, activeLow ? 1 : 0,
             static_cast<unsigned>(levelWithoutLiquidOut), static_cast<unsigned>(levelWithLiquid),
             static_cast<unsigned long>(calibratedAt));
    if (!persistSensor(sensorIndex, [levelWithoutLiquidOut, levelWithLiquid, activeLow,
                                     calibratedAt](Xkc25SensorConfig& cfg) {
            // Уровень покоя может быть измерен в другом режиме входа, чем был записан
            // на шаге 1, поэтому пишем оба уровня одним обновлением.
            cfg.levelWithoutLiquid = levelWithoutLiquidOut;
            cfg.levelWithLiquid = levelWithLiquid;
            cfg.activeLow = activeLow;
            cfg.calibrated = true;
            cfg.calibratedAt = calibratedAt;
        })) {
        ESP_LOGE(TAG, "Sensor #%u: failed to save calibration",
                 static_cast<unsigned>(sensorIndex) + 1);
        result.code = Xkc25CalibrationCode::PERSIST_FAILED;
        result.message = "Не удалось сохранить калибровку";
        return result;
    }

    sensor->applyCalibration(activeLow);

    result.ok = true;
    result.completed = true;
    result.message = "Калибровка завершена";
    return result;
}

Xkc25CalibrationResult Xkc25Subsystem::verifyReturn(const uint8_t sensorIndex, const uint32_t timeoutMs) {
    Xkc25CalibrationResult result;
    Xkc25Sensor* sensor = findSensor(sensorIndex);
    if (sensor == nullptr) {
        result.code = Xkc25CalibrationCode::SENSOR_NOT_AVAILABLE;
        result.message = "Датчик не найден или выключен";
        return result;
    }

    uint8_t levelWithoutLiquid = 0;
    bool calibrated = false;
    bool found = false;
    OpenConnectHardwareSettingsService::instance().read([&](OpenConnectHardwareConfig& cfg) {
        if (sensorIndex < cfg.xkc25Sensors.size()) {
            const Xkc25SensorConfig& sensorCfg = cfg.xkc25Sensors[sensorIndex];
            levelWithoutLiquid = sensorCfg.levelWithoutLiquid;
            calibrated = sensorCfg.calibrated;
            found = true;
        }
    });
    if (!found || !calibrated) {
        result.code = Xkc25CalibrationCode::NOT_CALIBRATED;
        result.message = "Сначала выполните калибровку датчика";
        return result;
    }

    const uint32_t effectiveTimeout = timeoutMs == 0 ? kVerifyReturnDefaultTimeoutMs : timeoutMs;
    const uint32_t start = millis();
    int raw = sensor->readRawPinLevel();
    while (raw != static_cast<int>(levelWithoutLiquid) &&
           static_cast<uint32_t>(millis() - start) < effectiveTimeout) {
        delay(kVerifyReturnPollMs);
        raw = sensor->readRawPinLevel();
    }

    result.rawLevel = raw;
    result.ok = (raw == static_cast<int>(levelWithoutLiquid));
    result.code = result.ok ? Xkc25CalibrationCode::OK : Xkc25CalibrationCode::VERIFY_FAILED;
    // Проверка не блокирующая: калибровка сохраняется, а результат — только предупреждение.
    result.message = result.ok
                         ? "Датчик вернулся в состояние без жидкости"
                         : "Датчик не вернулся в исходное состояние: залипание выхода или уровень не опустился";
    return result;
}

bool Xkc25Subsystem::hasUncalibratedEnabledSensor() {
    bool found = false;
    OpenConnectHardwareSettingsService::instance().read([&](OpenConnectHardwareConfig& cfg) {
        for (const auto& sensor : cfg.xkc25Sensors) {
            if (sensor.enabled && !sensor.calibrated) {
                found = true;
                return;
            }
        }
    });
    return found;
}

Xkc25StartCheck Xkc25Subsystem::checkStart() {
    Xkc25StartCheck check;
    std::vector<std::string> uncalibrated;

    OpenConnectHardwareSettingsService::instance().read([&](OpenConnectHardwareConfig& cfg) {
        check.mode = cfg.xkc25StartGuardMode;
        if (check.mode == OpenConnectHardwareConfig::kXkc25GuardOff) {
            return;
        }

        for (size_t slot = 0; slot < cfg.xkc25Sensors.size(); ++slot) {
            const Xkc25SensorConfig& sensor = cfg.xkc25Sensors[slot];
            if (!sensor.enabled) {
                continue;
            }

            // У переносного датчика зона — это ёмкость, на которой он стоит сейчас.
            const uint8_t zoneIndex =
                sensor.sharedAcrossTanks ? cfg.xkc25SharedTankIndex : sensor.zoneIndex;
            const std::string inputLabel = sensorInputLabel(sensor);

            if (!sensor.calibrated) {
                uncalibrated.push_back("Датчик уровня #" + std::to_string(slot + 1) + " (" +
                                       inputLabel + ", " + zoneLabel(zoneIndex) +
                                       ") не откалиброван — работа датчика не проверена");
                continue;
            }

            // Уровней всего два, поэтому любое текущее значение совпадёт с одним из
            // калиброванных: проверяем только «уже заполнено» (6.4.4).
            bool filled = false;
            bool readOk = true;
            if (Xkc25Subsystem* active = _active) {
                if (Xkc25Sensor* live = active->findSensor(static_cast<uint8_t>(slot))) {
                    filled = live->isFilled();
                }
            } else {
                DigitalInputSource src;
                src.source = sensor.pinSource;
                src.gpio = sensor.gpio;
                src.pcfAddress = sensor.pcfAddress;
                src.pcfBit = sensor.pcfBit;
                const int raw = DigitalInputPin::readOnce(src, readOk);
                filled = readOk && (sensor.activeLow ? (raw == LOW) : (raw == HIGH));
            }
            if (!readOk) {
                // Сбой чтения (шина I²C) — не «заполнено»: сообщаем отдельно, не блокируем.
                check.warnings.push_back(inputLabel + ": не удалось прочитать вход (сбой шины I²C)");
            } else if (filled) {
                // Последствие зависит от выбранного режима: пауза — не единственный вариант.
                const std::string consequence =
                    cfg.xkc25TriggerAction == OpenConnectHardwareConfig::kXkc25ActionAbort
                        ? "при старте ректификация сразу прервётся"
                        : (cfg.xkc25TriggerAction == OpenConnectHardwareConfig::kXkc25ActionNotify
                               ? "при срабатывании будет только уведомление"
                               : "отбор сразу встанет на паузу");
                check.warnings.push_back(zoneLabel(zoneIndex) + " уже заполнена — " + consequence);
            }
        }
    });

    if (check.mode == OpenConnectHardwareConfig::kXkc25GuardBlock && !uncalibrated.empty()) {
        check.allow = false;
        check.blockReasons = uncalibrated;
    } else {
        for (const auto& reason : uncalibrated) {
            check.warnings.push_back(reason);
        }
    }
    return check;
}

void Xkc25Subsystem::checkProcessTransition() {
    if (!_enabled) {
        return;
    }

    // Переносной датчик переключён в UI на другую ёмкость — применяем зону на лету.
    syncSharedTankZone();

    RectificationProcess::Snapshot snapshot;
    if (!RectificationProcess::rectController().getSnapshot(snapshot)) {
        return;
    }

    const bool running = snapshot.state == RectificationTypes::ProcessState::RUNNING;
    const bool justStarted = running && !_processWasRunning;
    _processWasRunning = running;
    if (!justStarted || !hasUncalibratedEnabledSensor()) {
        return;
    }

    // Старт физической кнопкой SSVC-0059 предотвратить нельзя, поэтому процесс не
    // прерываем и на паузу не ставим — только уведомление и запись в журнал.
    pushWarning("Ректификация запущена без калибровки датчиков уровня");
}

void Xkc25Subsystem::syncSharedTankZone() {
    uint8_t tankIndex = 0;
    OpenConnectHardwareSettingsService::instance().read([&](OpenConnectHardwareConfig& cfg) {
        tankIndex = cfg.xkc25SharedTankIndex;
    });

    for (Xkc25Sensor* sensor : _sensors) {
        if (sensor == nullptr || !sensor->isSharedAcrossTanks()) {
            continue;
        }
        if (sensor->getZoneIndex() == tankIndex) {
            continue;
        }

        // Датчик физически переставлен: меняется только зона, калибровка не затрагивается.
        sensor->setZoneIndex(tankIndex);
        ESP_LOGI(TAG, "[%s] Portable sensor moved to %s", sensor->getName().c_str(),
                 zoneLabel(tankIndex).c_str());
        pushWarning("Датчик уровня переключён на " + zoneLabel(tankIndex) + " (" +
                    sensor->pinLabel() + ")");
    }
}

void Xkc25Subsystem::runBootAutocheck() {
    std::vector<size_t> inconsistent;
    OpenConnectHardwareSettingsService::instance().read([&](OpenConnectHardwareConfig& cfg) {
        for (size_t slot = 0; slot < cfg.xkc25Sensors.size(); ++slot) {
            const Xkc25SensorConfig& sensor = cfg.xkc25Sensors[slot];
            // Совпавшие уровни мастер калибровки сохранить не может, значит конфиг
            // правили руками и достоверной калибровки у датчика нет.
            if (sensor.enabled && sensor.calibrated &&
                sensor.levelWithoutLiquid == sensor.levelWithLiquid) {
                inconsistent.push_back(slot);
            }
        }
    });

    if (inconsistent.empty()) {
        return; // Конфиг целостен — на FS не пишем вовсе, карта памяти не изнашивается (6.1.4).
    }

    OpenConnectHardwareSettingsService::instance().update(
        [&](OpenConnectHardwareConfig& cfg) -> StateUpdateResult {
            bool changed = false;
            for (const size_t slot : inconsistent) {
                if (slot >= cfg.xkc25Sensors.size() || !cfg.xkc25Sensors[slot].calibrated) {
                    continue;
                }
                cfg.xkc25Sensors[slot].calibrated = false;
                cfg.xkc25Sensors[slot].calibratedAt = 0;
                changed = true;
            }
            return changed ? StateUpdateResult::CHANGED : StateUpdateResult::UNCHANGED;
        },
        "xkc25_autocheck");

    for (const size_t slot : inconsistent) {
        ESP_LOGW(TAG, "Level sensor #%u: calibration reset (equal levels) — recalibrate it",
                 static_cast<unsigned>(slot + 1));
    }
}
