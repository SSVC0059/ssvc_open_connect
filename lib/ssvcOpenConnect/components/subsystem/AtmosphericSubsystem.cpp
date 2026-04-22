#include "AtmosphericSubsystem.h"
#include "core/SubsystemManager/SubsystemManager.h"
#include "core/StatefulServices/OpenConnectHardwareSettingsService/OpenConnectHardwareConfig.h"
#include "core/StatefulServices/OpenConnectHardwareSettingsService/OpenConnectHardwareSettingsService.h"
#include "components/sensors/SensorManager/SensorManager.h"
#include <esp_log.h>

#include "components/sensors/SensorCoordinator/SensorCoordinator.h"

void AtmosphericSubsystem::initialize() {
    _initialized = true;
    ESP_LOGI(TAG, "Atmospheric subsystem initialized");
}

void AtmosphericSubsystem::enable() {
    if (!_initialized || _enabled) return;

    I2CBusSubsystem& i2cBus = I2CBusSubsystem::getInstance();

    // Проверяем, что шина I2C активна. SubsystemManager должен был включить её раньше.
    if (!i2cBus.isActive()) {
        ESP_LOGW(TAG, "I2C_BUS is not active. AtmosphericSubsystem cannot start.");
        SubsystemManager::instance().disableSubsystem(getName()); // Отключаем себя
        return;
    }

    uint8_t sensorAddr = 0x47;
    OpenConnectHardwareSettingsService::instance().read([&](OpenConnectHardwareConfig& cfg) {
        sensorAddr = cfg.bmp581I2cAddress;
    });

    _sensor = new BMP581Sensor(sensorAddr, "Давление", &i2cBus);

    if (_sensor->initialize()) {
        SensorManager::getInstance().registerSensor(_sensor);
        _enabled = true;
        ESP_LOGI(TAG, "BMP581 Sensor successfully started");
        SensorCoordinator::getInstance().registerPollingSubsystem(_sensor);
    } else {
        ESP_LOGE(TAG, "Failed to initialize BMP581 hardware. Disabling subsystem.");
        delete _sensor;
        _sensor = nullptr;
        SubsystemManager::instance().disableSubsystem(getName());
    }
}

void AtmosphericSubsystem::disable() {
    if (!_enabled) return;
    _enabled = false;
    ESP_LOGI(TAG, "Atmospheric subsystem disabled");
}