#include "DS18B20Sensor.h"
#include <iomanip>
#include <sstream>

// Добавляем определение тега логгирования
static constexpr const char* TAG = "DS18B20Sensor";

DS18B20Sensor::DS18B20Sensor(DallasTemperature &sensors,
                             const DeviceAddress &address)
    : sensors(sensors), address(address)
{
    ESP_LOGD(TAG, "Creating sensor for address: %s",
            getAddressString().c_str());

    // Проверка подключения датчика
    if (!sensors.isConnected(address.data())) {
        ESP_LOGE(TAG, "Sensor %s not connected!", getAddressString().c_str());
    } else {
        ESP_LOGI(TAG, "Sensor %s initialized successfully",
                getAddressString().c_str());

        // Устанавливаем максимальное разрешение (12 бит)
        sensors.setResolution(address.data(), 12);
        ESP_LOGD(TAG, "Resolution set to %d bits",
                sensors.getResolution(address.data()));
    }
}

float DS18B20Sensor::getTemperatureC() const {

    ESP_LOGD(TAG, "Getting temperature for %s", getAddressString().c_str());

    if (!sensors.isConnected(address.data())) {
        ESP_LOGW(TAG, "Sensor %s disconnected!", getAddressString().c_str());
        return DEVICE_DISCONNECTED_C;
    }

    if (!sensors.isConversionComplete()) {
        ESP_LOGW(TAG, "[%s] Reading before conversion complete",
                getAddressString().c_str());
        return DEVICE_DISCONNECTED_C;
    }

    float temp = sensors.getTempC(address.data());

    // Дополнительная проверка значения
    if (temp == DEVICE_DISCONNECTED_C) {
        ESP_LOGE(TAG, "[%s] Error reading temperature",
                getAddressString().c_str());
    }
    else if (temp == 85.0f) {
        ESP_LOGW(TAG, "[%s] Got default value 85°C - check timing",
                getAddressString().c_str());
    }
    else {
        ESP_LOGD(TAG, "[%s] Temperature: %.2f°C",
                getAddressString().c_str(), temp);
    }

    return temp;
}

// Остальные методы без изменений
const DS18B20Sensor::DeviceAddress &DS18B20Sensor::getAddress() const {
    return address;
}

const std::string &DS18B20Sensor::getAddressString() const {
    if (cachedAddress.empty()) {
        std::ostringstream oss;
        oss << std::hex << std::setfill('0');
        for (size_t i = 0; i < address.size(); ++i) {
            if (i != 0) oss << '-';
            oss << std::setw(2) << static_cast<int>(address[i]);
        }
        cachedAddress = oss.str();
    }
    return cachedAddress;
}

std::string DS18B20Sensor::getAddressStringDelimited(char delimiter) const {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (size_t i = 0; i < address.size(); ++i) {
        if (i != 0) oss << delimiter;
        oss << std::setw(2) << static_cast<int>(address[i]);
    }
    return oss.str();
}