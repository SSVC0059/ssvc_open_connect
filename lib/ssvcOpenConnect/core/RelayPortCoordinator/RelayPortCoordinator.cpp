#include "core/SsvcConnector.h"

#if !PINOUT_USE_GPIO

#include "RelayPortCoordinator.h"
#include "core/StatefulServices/OpenConnectHardwareSettingsService/OpenConnectHardwareConfig.h"
#include <esp_log.h>

namespace {

static const char* TAG = "RelayPortCoord";

static const char* const kRelayChipRoles[OpenConnectHardwareConfig::kMaxRelayChips] = {
    SSVC_RELAY_PCF8574_DEVICE_ROLE,
    "pcf8574_port1",
    "pcf8574_port2",
    "pcf8574_port3",
    "pcf8574_port4",
    "pcf8574_port5",
    "pcf8574_port6",
    "pcf8574_port7",
};

} // namespace

RelayPortCoordinator& RelayPortCoordinator::getInstance() {
  static RelayPortCoordinator instance;
  return instance;
}

RelayPortCoordinator::RelayPortCoordinator() = default;

void RelayPortCoordinator::configure(const std::vector<uint8_t>& addresses) {
  _ports.clear();
  _addresses.clear();
  _userShadows.clear();

  std::vector<uint8_t> addrs = addresses;
  if (addrs.empty()) {
    addrs.push_back(SSVC_RELAY_PCF8574_I2C_ADDR);
  }
  if (addrs.size() > OpenConnectHardwareConfig::kMaxRelayChips) {
    addrs.resize(OpenConnectHardwareConfig::kMaxRelayChips);
  }

  _addresses = std::move(addrs);
  _userShadows.assign(_addresses.size(), 0xFF);
  _alarmByte = 0xFF;

  for (size_t i = 0; i < _addresses.size(); ++i) {
    const char* role = kRelayChipRoles[i];
    _ports.emplace_back(_addresses[i], role);
  }

  ESP_LOGI(TAG, "configure: %u PCF8574 relay chip(s)", static_cast<unsigned>(_ports.size()));
}

unsigned RelayPortCoordinator::totalRelayLines() const {
  return static_cast<unsigned>(_ports.size() * SSVC_RELAY_PCF8574_LINES_PER_CHIP);
}

uint8_t RelayPortCoordinator::reservedAlarmBitsMask() const { return alarmMask(); }

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

bool RelayPortCoordinator::isAlarmReservedGlobal(const unsigned globalBit) const {
  const unsigned chip = globalBit / SSVC_RELAY_PCF8574_LINES_PER_CHIP;
  if (chip != 0) {
    return false;
  }
  const unsigned local = globalBit % SSVC_RELAY_PCF8574_LINES_PER_CHIP;
  const uint8_t bit = static_cast<uint8_t>(1u << local);
  return (alarmMask() & bit) != 0;
}

void RelayPortCoordinator::setUserLine(const unsigned globalBitIndex, const bool energized) {
  if (_ports.empty() || globalBitIndex >= totalRelayLines()) {
    return;
  }
  const unsigned chip = globalBitIndex / SSVC_RELAY_PCF8574_LINES_PER_CHIP;
  const unsigned local = globalBitIndex % SSVC_RELAY_PCF8574_LINES_PER_CHIP;
  if (isAlarmReservedGlobal(globalBitIndex)) {
    return;
  }
  const uint8_t bit = static_cast<uint8_t>(1u << local);
  uint8_t& shadow = _userShadows[chip];
  if (energized) {
    shadow &= static_cast<uint8_t>(~bit);
  } else {
    shadow |= bit;
  }
}

uint8_t RelayPortCoordinator::userShadow() const {
  if (_userShadows.empty()) {
    return 0xFF;
  }
  return _userShadows[0];
}

uint8_t RelayPortCoordinator::userShadowChip(const unsigned chipIndex) const {
  if (chipIndex >= _userShadows.size()) {
    return 0xFF;
  }
  return _userShadows[chipIndex];
}

Pcf8574RelayPort& RelayPortCoordinator::portAt(const size_t chipIndex) { return _ports.at(chipIndex); }

const Pcf8574RelayPort& RelayPortCoordinator::portAt(const size_t chipIndex) const {
  return _ports.at(chipIndex);
}

bool RelayPortCoordinator::flush() {
  if (_ports.empty()) {
    return true;
  }
  bool allOk = true;
  for (size_t i = 0; i < _ports.size(); ++i) {
    uint8_t merged = _userShadows[i];
    if (i == 0) {
      merged = static_cast<uint8_t>((merged & userMaskChip0()) | (_alarmByte & alarmMask()));
    }
    _ports[i].setShadowRaw(merged);
    if (!_ports[i].flush()) {
      allOk = false;
      ESP_LOGW(TAG, "PCF8574 flush failed chip=%u", static_cast<unsigned>(i));
    }
  }
  return allOk;
}

#endif
