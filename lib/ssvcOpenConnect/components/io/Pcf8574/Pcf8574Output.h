#ifndef SSVC_OPEN_CONNECT_PCF8574_OUTPUT_H
#define SSVC_OPEN_CONNECT_PCF8574_OUTPUT_H

#include <Arduino.h>
#include <Wire.h>

/**
 * Minimal PCF8574 output driver: single I2C write of the 8-bit port.
 * Does not call Wire.begin; uses the bus provided by I2CBusSubsystem.
 */
class Pcf8574Output {
public:
  Pcf8574Output() = default;
  Pcf8574Output(TwoWire* wire, uint8_t i2cAddress) : _wire(wire), _addr(i2cAddress) {}

  void setBus(TwoWire* wire) { _wire = wire; }
  void setAddress(uint8_t i2cAddress) { _addr = i2cAddress; }

  /** @return true if ACK received */
  bool writePort(uint8_t portValue) const;

private:
  TwoWire* _wire = nullptr;
  uint8_t _addr = 0x24;
};

#endif
