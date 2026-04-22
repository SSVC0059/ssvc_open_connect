#ifndef SSVC_OPEN_CONNECT_PCF8574_RELAY_PORT_H
#define SSVC_OPEN_CONNECT_PCF8574_RELAY_PORT_H

#include <Arduino.h>
#include <Wire.h>
#include "components/io/Pcf8574/Pcf8574Output.h"

/**
 * One PCF8574 used as an output port (e.g. relay drivers). Board-agnostic: address and
 * AlarmMonitor role are supplied per instance. Several chips on the bus = several instances
 * with distinct I2C addresses and device roles.
 *
 * Convention: active-LOW on the port bit = relay energized (common module wiring).
 */
class Pcf8574RelayPort {
public:
  Pcf8574RelayPort(uint8_t i2cAddr, const char* deviceRoleForAlarms);

  uint8_t i2cAddress() const { return _i2cAddr; }
  const char* deviceRole() const { return _deviceRole; }

  uint8_t shadow() const { return _shadow; }

  /** Write shadow to the chip; notifies AlarmMonitor on I2C failure / recovery. */
  bool flush();

  /** One output bit (0..7); energized=true pulls line active (here: clears bit). */
  void setOutputEnergized(unsigned bitIndex, bool energized);

  void setShadowAllReleased();

  /** Replace full port shadow (e.g. merged alarm + user lines). */
  void setShadowRaw(uint8_t shadow) { _shadow = shadow; }

private:
  uint8_t _i2cAddr;
  const char* _deviceRole;
  Pcf8574Output _pcf;
  uint8_t _shadow = 0xFF;
};

#endif
