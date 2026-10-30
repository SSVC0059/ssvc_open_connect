#include "Pcf8574Input.h"
#include <esp_log.h>

namespace {
constexpr auto TAG = "Pcf8574Input";
}

bool Pcf8574Input::prepareInputs() {
  if (_wire == nullptr) {
    ESP_LOGW(TAG, "prepareInputs: Wire is null");
    return false;
  }
  _wire->beginTransmission(_addr);
  _wire->write(static_cast<uint8_t>(0xFF));
  const uint8_t err = _wire->endTransmission();
  if (err != 0) {
    ESP_LOGE(TAG, "PCF8574 input prepare failed addr=0x%02X err=%u", _addr,
             static_cast<unsigned>(err));
    return false;
  }
  ESP_LOGD(TAG, "PCF8574 0x%02X inputs armed (0xFF)", _addr);
  return true;
}

bool Pcf8574Input::readPort(uint8_t& value) const {
  value = 0xFF;
  if (_wire == nullptr) {
    ESP_LOGW(TAG, "readPort: Wire is null");
    return false;
  }
  if (_wire->requestFrom(_addr, static_cast<uint8_t>(1)) != 1) {
    // NACK or short transfer: the level is unknown — the caller must not treat it as 0.
    ESP_LOGE(TAG, "PCF8574 input read failed addr=0x%02X (no ACK)", _addr);
    return false;
  }
  if (_wire->available() < 1) {
    ESP_LOGW(TAG, "PCF8574 input read addr=0x%02X: no bytes available", _addr);
    return false;
  }
  value = static_cast<uint8_t>(_wire->read());
  ESP_LOGD(TAG, "PCF8574 0x%02X => 0x%02X", _addr, value);
  return true;
}

int Pcf8574Input::readBit(const uint8_t bit, bool& ok) const {
  uint8_t port = 0xFF;
  ok = readPort(port);
  if (!ok) {
    return -1;
  }
  const uint8_t mask = static_cast<uint8_t>(1u << (bit & 0x07u));
  return (port & mask) ? 1 : 0;
}
