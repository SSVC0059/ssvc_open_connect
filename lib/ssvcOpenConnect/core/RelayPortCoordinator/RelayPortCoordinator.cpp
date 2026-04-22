#include "core/SsvcConnector.h"

#if !PINOUT_USE_GPIO

#include "RelayPortCoordinator.h"
#include <esp_log.h>

static const char* TAG = "RelayPortCoord";

RelayPortCoordinator& RelayPortCoordinator::getInstance() {
  static RelayPortCoordinator instance;
  return instance;
}

RelayPortCoordinator::RelayPortCoordinator()
    : _port(SSVC_RELAY_PCF8574_I2C_ADDR, SSVC_RELAY_PCF8574_DEVICE_ROLE) {}

uint8_t RelayPortCoordinator::alarmMask() const {
  return static_cast<uint8_t>((1u << SSVC_RELAY_PCF8574_BIT_DANGEROUS) |
                              (1u << SSVC_RELAY_PCF8574_BIT_CRITICAL));
}

uint8_t RelayPortCoordinator::buildAlarmByte(AlarmLevel level) const {
  uint8_t b = 0xFF;
  if (level == AlarmLevel::DANGEROUS) {
    b &= static_cast<uint8_t>(~(1u << SSVC_RELAY_PCF8574_BIT_DANGEROUS));
  } else if (level == AlarmLevel::CRITICAL) {
    b &= static_cast<uint8_t>(~(1u << SSVC_RELAY_PCF8574_BIT_CRITICAL));
  }
  return b;
}

void RelayPortCoordinator::setAlarmLevel(AlarmLevel level) { _alarmByte = buildAlarmByte(level); }

void RelayPortCoordinator::resetAlarmLines() { _alarmByte = 0xFF; }

void RelayPortCoordinator::setUserLine(unsigned bitIndex, bool energized) {
  if (bitIndex > 7) {
    return;
  }
  const uint8_t bit = static_cast<uint8_t>(1u << bitIndex);
  if (alarmMask() & bit) {
    return;
  }
  if (energized) {
    _userShadow &= static_cast<uint8_t>(~bit);
  } else {
    _userShadow |= bit;
  }
}

uint8_t RelayPortCoordinator::mergedShadow() const {
  return static_cast<uint8_t>((_userShadow & userMask()) | (_alarmByte & alarmMask()));
}

bool RelayPortCoordinator::flush() {
  const uint8_t m = mergedShadow();
  _port.setShadowRaw(m);
  const bool ok = _port.flush();
  if (!ok) {
    ESP_LOGW(TAG, "PCF8574 flush failed");
  }
  return ok;
}

#endif
