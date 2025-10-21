#include "OneWireThermalSubsystem.h"

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

#include "components/sensors/SensorManager/SensorManager.h"
#include "core/SsvcOpenConnect.h"

OneWireThermalSubsystem& OneWireThermalSubsystem::getInstance() {
    static OneWireThermalSubsystem instance;
    return instance;
}

bool OneWireThermalSubsystem::initialize() {
    ESP_LOGV(TAG, "Initializing OneWire Thermal Subsystem on pin %d.", ONEWIRE_PIN);

    // 1. Инициализация аппаратных ресурсов
    oneWireBus = new OneWire(ONEWIRE_PIN);
    dallasTemp = new DallasTemperature(oneWireBus);
    
    // Начало работы DallasTemperature
    dallasTemp->begin();
    ESP_LOGV(TAG, "DallasTemperature started successfully.");

    // 2. Поиск и регистрация датчиков
    discoverAndRegisterSensors();

    const auto* ssvcConnect = &SsvcOpenConnect::getInstance();

    if (auto* zoneService = ssvcConnect->getSensorZoneService()) {

        zoneService->read([&](const SensorZoneState& state) {
            ESP_LOGI(TAG, "[ZONE_CONFIG] Loaded zones in memory: %zu entries.", state.sensor_zones.size());
            for (const auto& pair : state.sensor_zones) {
                ESP_LOGI(TAG, "[ZONE_CONFIG] Address: %s, Zone: %d",
                         pair.first.c_str(), static_cast<int>(pair.second));
            }

            // Вызываем метод applyZonesToSensors
            state.applyZonesToSensors();
        });

        ESP_LOGI(TAG, "Applied saved SensorZone configurations to %zu sensors after discovery.", SensorManager::getInstance().getRegisteredSensorCount());
    } else {
        ESP_LOGE(TAG, "Cannot apply SensorZone configuration: SensorZoneService not initialized/accessible.");
    }
    
    return true;
}

void OneWireThermalSubsystem::discoverAndRegisterSensors(){
    // 1. Поиск
    const int deviceCount = dallasTemp->getDeviceCount();
    ESP_LOGV(TAG, "Starting sensor discovery. Found %d DS18B20 devices on bus.", deviceCount);

    AbstractSensor::Address addr;
    SensorManager& manager = SensorManager::getInstance();

    // Очищаем внутренний список перед новым обнаружением (для переинициализации)
    ds18b20Sensors.clear();

    for (int i = 0; i < deviceCount; i++) {
        if (dallasTemp->getAddress(addr, i)) {

            // 1. Получаем полную строку адреса
            const std::string fullAddressStr = SensorManager::addressToString(addr);

            // 2. Вычисляем начальную позицию (длина строки - 4)
            // 1-Wire адрес состоит из 16 символов (8 байт в шестнадцатеричной форме),
            // поэтому startPos = 16 - 4 = 12.
            const size_t startPos = fullAddressStr.length() > 4 ? fullAddressStr.length() - 4 : 0;

            // 3. Извлекаем последние 4 символа
            const std::string lastFourDigits = fullAddressStr.substr(startPos);

            char nameBuffer[20];
            sprintf(nameBuffer, "DS_%s_%d", lastFourDigits.c_str(), i + 1);

            ESP_LOGV(TAG, "Device found: %s at index %d.", nameBuffer, i);

            // 2. Создание и регистрация
            auto newSensor = new DS18B20Sensor(
                addr,
                nameBuffer,
                dallasTemp,
                SensorZone::UNKNOWN
            );

            // Регистрируем в абстрактном SensorManager
            manager.registerSensor(newSensor);

            // Сохраняем указатель в специфичном списке
            ds18b20Sensors.push_back(newSensor);
            
            ESP_LOGV(TAG, "Sensor %s successfully registered and added to poll list.", nameBuffer);
        } else {
            ESP_LOGW(TAG, "Could not get address for device at index %d.", i);
        }
    }
    ESP_LOGV(TAG, "Sensor discovery finished. %zu DS18B20 sensors registered.", ds18b20Sensors.size());
}


// --- Цикл Опроса RTOS ---

void OneWireThermalSubsystem::poll() {
    // 1. Фаза 1: Запрос температуры (специфика 1-Wire)
    if (dallasTemp) {
        ESP_LOGV(TAG, "Polling cycle started. Requesting temperatures for all %zu devices.", ds18b20Sensors.size());
        
        // Запускает преобразование (блокировка на ~750мс для 12-бит)
        dallasTemp->requestTemperatures(); 
        
        ESP_LOGV(TAG, "Temperature conversion initiated/completed.");
    }

    // 2. Фаза 2: Считывание и обновление состояния
    // Итерируемся по внутреннему списку DS18B20Sensor*.
    int readCount = 0;
    for (DS18B20Sensor* dsSensor : ds18b20Sensors) {
        
        // Вызываем readValue(), который использует dallasTemp внутри себя
        dsSensor->readValue(); 
        
        // Логируем результат, используя getData()
        const float temp = dsSensor->getData();
        const std::string address = SensorManager::addressToString(dsSensor->getAddress());
        
        ESP_LOGV(TAG, "Read temp %.2f C for sensor %s.", temp, address.c_str());
        
        if (temp == DEVICE_DISCONNECTED_C) {
             ESP_LOGW(TAG, "Sensor %s returned DEVICE_DISCONNECTED_C (Error: %.2f).", address.c_str(), temp);
        }
        readCount++;
    }
    ESP_LOGV(TAG, "Polling cycle finished. %d sensor values processed.", readCount);
}

// ... (Деструктор и остальные методы PollingSubsystem.cpp) ...