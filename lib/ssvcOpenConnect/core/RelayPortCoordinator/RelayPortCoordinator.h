#ifndef SSVC_OPEN_CONNECT_RELAY_PORT_COORDINATOR_H
#define SSVC_OPEN_CONNECT_RELAY_PORT_COORDINATOR_H

#include <Arduino.h>
#include "core/SsvcConnector.h"

#if !PINOUT_USE_GPIO
#include "components/io/Pcf8574RelayPort.h"
#include "core/IAlarmSubscriber/IAlarmSubscriber.h"

/**
 * Merges AlarmManager lines (DANGEROUS / CRITICAL bits) with user-configurable lines on one PCF8574.
 * Alarm bits are taken from SSVC_RELAY_PCF8574_BIT_*; all other bits are owned by user relay logic.
 */
class RelayPortCoordinator {
public:
  static RelayPortCoordinator& getInstance();

  /** Update alarm outputs from a sensor alarm (same semantics as legacy PinOutSubscriber). */
  void setAlarmLevel(AlarmLevel level);

  /** Release both alarm lines (idle). */
  void resetAlarmLines();

  /** User line 0..7; bits reserved for alarm are ignored. */
  void setUserLine(unsigned bitIndex, bool energized);

  uint8_t userShadow() const { return _userShadow; }

  /** Merge alarm + user and write I2C. */
  bool flush();

  Pcf8574RelayPort& port() { return _port; }

  uint8_t reservedAlarmBitsMask() const { return alarmMask(); }
  uint8_t userBitsMask() const { return userMask(); }

private:
  RelayPortCoordinator();
  uint8_t alarmMask() const;
  uint8_t userMask() const { return static_cast<uint8_t>(~alarmMask() & 0xFF); }
  uint8_t buildAlarmByte(AlarmLevel level) const;
  uint8_t mergedShadow() const;

  Pcf8574RelayPort _port;
  uint8_t _alarmByte = 0xFF;
  uint8_t _userShadow = 0xFF;
};

#else

/** GPIO alarm build: no PCF8574 merge. */
class RelayPortCoordinator {
public:
  static RelayPortCoordinator& getInstance() {
    static RelayPortCoordinator inst;
    return inst;
  }
};

#endif // !PINOUT_USE_GPIO

#endif // SSVC_OPEN_CONNECT_RELAY_PORT_COORDINATOR_H
