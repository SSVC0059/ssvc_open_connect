#ifndef SSVC_OPEN_CONNECT_PCF8574_INPUT_H
#define SSVC_OPEN_CONNECT_PCF8574_INPUT_H

#include <Arduino.h>
#include <Wire.h>

/**
 * Minimal PCF8574 input reader: I2C read of the 8-bit port.
 * Does not call Wire.begin; uses the bus provided by I2CBusSubsystem.
 *
 * PCF8574 is quasi-bidirectional: a bit reads as an input only while it is
 * written HIGH. A dedicated input expander must therefore be left in input
 * mode once via prepareInputs() (write 0xFF); reads then just request one byte.
 *
 * The bus is shared with the display, RTC and relay expanders: callers wrap
 * every transaction in I2CBusSubsystem::Guard (see Pcf8574RelayPort::flush).
 */
class Pcf8574Input {
public:
  Pcf8574Input() = default;
  Pcf8574Input(TwoWire* wire, uint8_t i2cAddress) : _wire(wire), _addr(i2cAddress) {}

  void setBus(TwoWire* wire) { _wire = wire; }
  void setAddress(uint8_t i2cAddress) { _addr = i2cAddress; }

  /** Write 0xFF so every bit becomes a quasi-bidirectional input. @return true on ACK */
  bool prepareInputs();

  /** Read the whole 8-bit port. @return true on ACK; @p value is valid only then. */
  bool readPort(uint8_t& value) const;

  /** Read one bit (0..7). @return level (0/1), or -1 when @p ok is false. */
  int readBit(uint8_t bit, bool& ok) const;

private:
  TwoWire* _wire = nullptr;
  uint8_t _addr = 0x22;
};

#endif // SSVC_OPEN_CONNECT_PCF8574_INPUT_H
