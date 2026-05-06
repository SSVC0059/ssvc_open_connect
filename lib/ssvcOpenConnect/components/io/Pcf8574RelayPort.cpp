#include "Pcf8574RelayPort.h"
#include "components/subsystem/I2CBusSubsystem.h"
#include "core/AlarmMonitor/AlarmMonitor.h"

Pcf8574RelayPort::Pcf8574RelayPort(const uint8_t i2cAddr, const char* deviceRoleForAlarms)
    : _i2cAddr(i2cAddr), _deviceRole(deviceRoleForAlarms ? deviceRoleForAlarms : "pcf8574") {
  _pcf.setAddress(_i2cAddr);
}

void Pcf8574RelayPort::setOutputEnergized(const unsigned bitIndex, const bool energized) {
  if (bitIndex > 7) {
    return;
  }
  if (energized) {
    _shadow &= ~static_cast<uint8_t>(1u << bitIndex);
  } else {
    _shadow |= static_cast<uint8_t>(1u << bitIndex);
  }
}

void Pcf8574RelayPort::setShadowAllReleased() { _shadow = 0xFF; }

bool Pcf8574RelayPort::flush() {
  TwoWire* w = I2CBusSubsystem::getInstance().getWire();
  if (w == nullptr) {
    AlarmMonitor::getInstance().raiseHardwareFault(
        HardwareFaultCode::I2C_BUS_DOWN, 0, "i2c_bus");
    return false;
  }
  _pcf.setBus(w);
  const bool ok = _pcf.writePort(_shadow);
  if (!ok) {
    AlarmMonitor::getInstance().raiseHardwareFault(
        HardwareFaultCode::I2C_NACK, _i2cAddr, _deviceRole);
  } else {
    AlarmMonitor::getInstance().clearHardwareFault(_deviceRole, _i2cAddr);
    AlarmMonitor::getInstance().clearHardwareFault("i2c_bus", 0);
  }
  return ok;
}
