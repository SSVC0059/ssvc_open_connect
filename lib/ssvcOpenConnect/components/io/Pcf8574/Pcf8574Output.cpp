#include "Pcf8574Output.h"
#include <esp_log.h>

namespace {
constexpr auto TAG = "Pcf8574Output";
}

bool Pcf8574Output::writePort(const uint8_t portValue) const {
  if (_wire == nullptr) {
    ESP_LOGW(TAG, "writePort: Wire is null");
    return false;
  }
  _wire->beginTransmission(_addr);
  _wire->write(portValue);
  const uint8_t err = _wire->endTransmission();
  if (err != 0) {
    ESP_LOGE(TAG, "PCF8574 write failed addr=0x%02X value=0x%02X err=%u",
             _addr, portValue, static_cast<unsigned>(err));
    return false;
  }
  ESP_LOGD(TAG, "PCF8574 0x%02X <= 0x%02X", _addr, portValue);
  return true;
}
