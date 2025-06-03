#ifndef THERMALSENSORS_H
#define THERMALSENSORS_H

#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <drivers/DS18B20/DS18B20Sensor.h>
#include <drivers/DS18B20/DS18B20Bus.h>
#include <ArduinoJson.h>
#include <core/OpenConnectSettings/OpenConnectSettings.h>
#include "ThermalSensorManager.h"
#include "ThermalSensorsConfig.h"
#include "core/OpenConnectSettings/GlobalConfig.h"


struct SensorTemperatureData
{
    std::string address; // Адрес датчика
    std::string zone; // Строковое описание зоны
    float temperature; // Температура
    bool valid; // флаг корректности датчика
};

struct ZoneInfo
{
    std::string zone;
    std::vector<std::string> addresses;
};

extern const char thermalSensorsConfigName[];

class ThermalSensors
{
public:
    static ThermalSensors& thermalController();
    ThermalSensors(const ThermalSensors&) = delete;
    ThermalSensors& operator=(const ThermalSensors&) = delete;

    void begin(OpenConnectSettingsService& openConnectSettingsService);

    size_t scanAndAddAllSensors(ThermalSensorZone defaultZone);
    std::unordered_map<std::string, std::vector<std::string>> getSensorAddressesByZone() const;
    std::unordered_map<std::string, std::vector<std::string>> getZonesWithAddresses() const;
    std::vector<SensorTemperatureData> getAllSensorTemperatures();

    // Основной интерфейс
    bool startContinuousReading(uint32_t intervalMs = 1000);
    void stopContinuousReading();
    std::vector<SensorTemperatureData> getTemperatures(bool forceUpdate = false);


    // Подписка на обновления
    // void temperatureUpdateCallback(const std::vector<SensorTemperatureData>& data) {
    //     Serial.println("New temperature data available:");
    //     for (const auto& sensor : data) {
    //         // Обработка новых данных
    //     }
    // }

    // Регистрация коллбэка
    //  ThermalSensors::thermalController().registerUpdateCallback(temperatureUpdateCallback);

    void registerUpdateCallback(const std::function<void(const std::vector<SensorTemperatureData>&)>& callback);

    // Диагностика
    struct Diagnostics {
        uint32_t lastReadTimeMs;
        uint32_t readErrors;
        uint32_t minReadTimeMs;
        uint32_t maxReadTimeMs;
        size_t sensorCount;
    };
    Diagnostics getDiagnostics() const;

    bool setSensorZone(const std::string& address, ThermalSensorZone zone);
    bool setSensorZone(const std::string& address, const std::string& zoneName);



private:
    explicit ThermalSensors();

    static void readingTask(void* params);
    void performTemperatureReading();
    static bool isValidTemperature(float temp) ;

    void loadConfig();
    void saveConfig() const;
    static void sensorConfigFromJson(const JsonObject& json, std::unordered_map<std::string, ThermalSensorZone>& config);
    static void sensorConfigToJson(const std::unordered_map<std::string, ThermalSensorZone>& config, JsonObject& json);

    // Инициализация статических объектов
    ThermalSensorManager _sensorManager;
    TaskHandle_t _readingTaskHandle = nullptr;
    SemaphoreHandle_t _dataMutex;
    std::vector<SensorTemperatureData> _cachedTemperatures;
    std::vector<std::function<void(const std::vector<SensorTemperatureData>&)>> _callbacks;

    // Диагностика
    uint32_t _lastReadTimeMs = 0;
    uint32_t _readErrors = 0;
    uint32_t _minReadTimeMs = UINT32_MAX;
    uint32_t _maxReadTimeMs = 0;

    // Настройки
    uint32_t _readingIntervalMs = 1000;
    bool _continuousReading = false;

    static ThermalSensors* _instance;
    static constexpr auto TAG = "ThermalSensors";

};

#endif //THERMALSENSORS_H
