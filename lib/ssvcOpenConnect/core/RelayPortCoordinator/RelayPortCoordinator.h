#ifndef SSVC_OPEN_CONNECT_RELAY_PORT_COORDINATOR_H
#define SSVC_OPEN_CONNECT_RELAY_PORT_COORDINATOR_H

#include <Arduino.h>
#include "core/SsvcConnector.h"
#include <cstdint>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <vector>

#if !PINOUT_USE_GPIO
#include "components/io/Pcf8574RelayPort.h"
#include "core/IAlarmSubscriber/IAlarmSubscriber.h"

/**
 * Merges AlarmManager lines (DANGEROUS / CRITICAL bits) on the first PCF8574 with user lines
 * across one or more expanders. Call `configure(addresses)` after hardware settings are loaded.
 */
class RelayPortCoordinator {
public:
  static RelayPortCoordinator& getInstance();

  /**
   * Rebuilds I2C ports from a 7-bit address list (order = chip index; first = alarm chip).
   * Empty `addresses` uses `SSVC_RELAY_PCF8574_I2C_ADDR` as a single chip.
   */
  void configure(const std::vector<uint8_t>& addresses);

  void setAlarmLevel(AlarmLevel level);
  void resetAlarmLines();

  /**
   * User line as global index: chip = index / 8, local = index % 8.
   * Lines reserved for alarms on the first chip are ignored.
   */
  void setUserLine(unsigned globalBitIndex, bool energized);

  /** @deprecated Use `userShadowChip` / `userShadows` in API; first chip only. */
  uint8_t userShadow() const;
  uint8_t userShadowChip(unsigned chipIndex) const;
  uint8_t effectiveShadowChip(unsigned chipIndex) const;
  size_t relayChipCount() const;
  unsigned totalRelayLines() const;
  std::vector<uint8_t> configuredAddresses() const;

  bool isAlarmReservedGlobal(unsigned globalBit) const;

  bool flush();

  Pcf8574RelayPort& portAt(size_t chipIndex);
  const Pcf8574RelayPort& portAt(size_t chipIndex) const;

  uint8_t reservedAlarmBitsMask() const;

private:
  RelayPortCoordinator();

  uint8_t alarmMask() const;
  uint8_t userMaskChip0() const { return static_cast<uint8_t>(~alarmMask() & 0xFF); }
  uint8_t buildAlarmByte(AlarmLevel level) const;
  uint8_t effectiveShadowChipLocked(unsigned chipIndex) const;

  std::vector<Pcf8574RelayPort> _ports;
  std::vector<uint8_t> _addresses;
  std::vector<uint8_t> _userShadows;
  uint8_t _alarmByte = 0xFF;
  mutable SemaphoreHandle_t _lock = nullptr;
};

#else

#include <vector>
#include <cstdint>

/** GPIO alarm build: no PCF8574 merge. */
class RelayPortCoordinator {
public:
  static RelayPortCoordinator& getInstance() {
    static RelayPortCoordinator inst;
    return inst;
  }
  void configure(const std::vector<uint8_t>&) { /* no-op: GPIO build bypasses PCF8574 coordination */ }
  unsigned totalRelayLines() const { return 0; }
  size_t relayChipCount() const { return 0; }
};

#endif // !PINOUT_USE_GPIO

#endif // SSVC_OPEN_CONNECT_RELAY_PORT_COORDINATOR_H
