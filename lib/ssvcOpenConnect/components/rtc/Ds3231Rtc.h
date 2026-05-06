#ifndef DS3231_RTC_H
#define DS3231_RTC_H

#include <Wire.h>
#include <ctime>
#include <cstdint>

/**
 * Minimal DS3231 driver over TwoWire — wall time stored as UTC (no TZ in hardware).
 */
class Ds3231Rtc {
public:
    void setBus(TwoWire* wire, uint8_t addr7bit) {
        _wire = wire;
        _addr = addr7bit;
    }

    bool probe() const;
    /** Read OSF (oscillator stop) from status reg; optional diagnostic. */
    bool readOscillatorStoppedFlag(bool& stopped) const;
    /** Read time as UTC calendar fields. Returns false on I²C failure. */
    bool readUtcTm(struct tm* outUtc) const;
    /** Write time from UTC calendar fields (DS3231 year 2000–2099). */
    bool writeUtcTm(const struct tm& utc) const;

private:
    bool readRegisters(uint8_t startReg, uint8_t* buf, size_t len) const;
    bool writeRegister(uint8_t reg, uint8_t val) const;

    TwoWire* _wire = nullptr;
    uint8_t _addr = 0x68;
};

#endif
