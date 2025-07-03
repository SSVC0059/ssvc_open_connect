#include "DS18B20Bus.h"

#include "commons/commons.h"

DS18B20Bus::DS18B20Bus(uint8_t pin) : oneWire(pin), sensors(&oneWire) {}

void DS18B20Bus::begin()
{
  ESP_LOGI(TAG, "Initializing DS18B20 bus");
  sensors.begin();

  // Проверка наличия датчиков
  uint8_t count = sensors.getDeviceCount();
  if (count == 0) {
    ESP_LOGW(TAG, "No sensors found!");
    return;
  }

  // Установка разрешения для всех датчиков
  DeviceAddress addr;
  for (uint8_t i = 0; i < count; i++) {
    if (sensors.getAddress(addr.data(), i)) {
      sensors.setResolution(addr.data(), 12, true);
      ESP_LOGD(TAG, "Sensor %d: %s, Resolution: %d bits",
              i,
              SensorAddressUtils::addressToString(addr.data()),
              sensors.getResolution(addr.data()));
    }
  }
}

std::vector<std::unique_ptr<DS18B20Sensor>> DS18B20Bus::discoverDevices() {
  std::vector<std::unique_ptr<DS18B20Sensor>> devices;
  DeviceAddress address;

  for (int i = 0; i < sensors.getDeviceCount(); i++) {
    if (sensors.getAddress(address.data(), i)) {
      if (address[0] == 0x28) {
        ESP_LOGI("DS18B20", "Found DS18B20 device");
        devices.emplace_back(std::make_unique<DS18B20Sensor>(sensors, address));
      } else {
        ESP_LOGI(TAG, "Обнаружен неизвестный датчик не семейства 0x%02X",
                 address[0]);
      }
    }
  }
  return devices;
}

DallasTemperature &DS18B20Bus::getSensorsInterface() { return sensors; }

bool DS18B20Bus::requestTemperatures(const bool waitForConversion) {
  // 1. Проверка наличия датчиков
  uint8_t deviceCount = sensors.getDeviceCount();
  if (deviceCount == 0) {
    ESP_LOGE(TAG, "No sensors detected on the bus!");
    return false;
  }

  // 2. Запрос температуры
  if (!sensors.requestTemperatures()) {
    ESP_LOGE(TAG, "Failed to send temperature request command!");
    return false;
  }

  _lastRequestTime = xTaskGetTickCount();
  _conversionStartTime = xTaskGetTickCount();
  _waitForConversion = waitForConversion;

  if (waitForConversion) {
    // Просто запоминаем время начала конверсии
    const uint16_t conversionTime = DallasTemperature::millisToWaitForConversion(sensors.getResolution());
    uint32_t _conversionTimeout = pdMS_TO_TICKS(conversionTime + 100);
    ESP_LOGV(TAG, "Conversion started, will take ~%d ms", conversionTime);
  }

  return true;
}

bool DS18B20Bus::isConversionComplete() const {
  if (!_waitForConversion) {
    return true; // Если не ждем конверсию, считаем что она завершена
  }

  // Проверяем таймаут
  if ((xTaskGetTickCount() - _conversionStartTime) >= _conversionTimeout) {
    ESP_LOGW(TAG, "Conversion timeout reached");
    return true;
  }

  // Эпизодическая проверка завершения конверсии (не каждый вызов)
  static uint32_t lastCheck = 0;
  if ((xTaskGetTickCount() - lastCheck) > pdMS_TO_TICKS(100)) {
    lastCheck = xTaskGetTickCount();
    return const_cast<DallasTemperature&>(sensors).isConversionComplete();
  }

  return false;
}