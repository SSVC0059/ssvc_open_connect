#include "Ds3231Rtc.h"

namespace {

uint8_t bcd2bin(uint8_t val) {
    return static_cast<uint8_t>(val - 6 * (val >> 4));
}

uint8_t bin2bcd(uint8_t val) {
    return static_cast<uint8_t>(val + 6 * (val / 10));
}

} // namespace

bool Ds3231Rtc::probe() const {
    if (!_wire) {
        return false;
    }
    _wire->beginTransmission(_addr);
    return _wire->endTransmission() == 0;
}

bool Ds3231Rtc::readRegisters(uint8_t startReg, uint8_t* buf, size_t len) const {
    if (!_wire || len == 0) {
        return false;
    }
    _wire->beginTransmission(_addr);
    _wire->write(startReg);
    if (_wire->endTransmission(false) != 0) {
        return false;
    }
    const size_t got = _wire->requestFrom(static_cast<uint8_t>(_addr), len);
    if (got != len) {
        return false;
    }
    for (size_t i = 0; i < len; ++i) {
        buf[i] = static_cast<uint8_t>(_wire->read());
    }
    return true;
}

bool Ds3231Rtc::writeRegister(uint8_t reg, uint8_t val) const {
    if (!_wire) {
        return false;
    }
    _wire->beginTransmission(_addr);
    _wire->write(reg);
    _wire->write(val);
    return _wire->endTransmission() == 0;
}

bool Ds3231Rtc::readOscillatorStoppedFlag(bool& stopped) const {
    uint8_t st = 0;
    if (!readRegisters(0x0F, &st, 1)) {
        return false;
    }
    stopped = (st & 0x80) != 0;
    return true;
}

bool Ds3231Rtc::readUtcTm(struct tm* outUtc) const {
    uint8_t buf[7];
    if (!readRegisters(0x00, buf, 7)) {
        return false;
    }
    if ((buf[0] & 0x80) != 0) {
        // CH (clock halt) — time may be invalid; still decode.
    }
    const int sec = bcd2bin(static_cast<uint8_t>(buf[0] & 0x7F));
    const int min = bcd2bin(static_cast<uint8_t>(buf[1] & 0x7F));
    int hour = bcd2bin(static_cast<uint8_t>(buf[2] & 0x3F));
    if ((buf[2] & 0x40) != 0) {
        // 12-hour mode — convert (boards should use 24h after writeUtcTm).
        const bool pm = (buf[2] & 0x20) != 0;
        hour = bcd2bin(static_cast<uint8_t>(buf[2] & 0x1F));
        if (hour == 12) {
            hour = pm ? 12 : 0;
        } else if (pm) {
            hour += 12;
        }
    }
    const int mday = bcd2bin(static_cast<uint8_t>(buf[4] & 0x3F));
    const int mon = bcd2bin(static_cast<uint8_t>(buf[5] & 0x1F));
    const int year2 = bcd2bin(buf[6]);
    const int fullYear = 2000 + year2;

    outUtc->tm_sec = sec;
    outUtc->tm_min = min;
    outUtc->tm_hour = hour;
    outUtc->tm_mday = mday;
    outUtc->tm_mon = mon - 1;
    outUtc->tm_year = fullYear - 1900;
    outUtc->tm_isdst = 0;
    return true;
}

bool Ds3231Rtc::writeUtcTm(const struct tm& utc) const {
    if (!_wire) {
        return false;
    }
    const int sec = utc.tm_sec;
    const int min = utc.tm_min;
    const int hour = utc.tm_hour;
    const int mday = utc.tm_mday;
    const int mon = utc.tm_mon + 1;
    const int fullYear = utc.tm_year + 1900;
    if (fullYear < 2000 || fullYear > 2099) {
        return false;
    }
    const uint8_t yr2 = static_cast<uint8_t>(fullYear - 2000);
    uint8_t dowRtc = 1;
    if (utc.tm_wday >= 0 && utc.tm_wday <= 6) {
        dowRtc = static_cast<uint8_t>(utc.tm_wday + 1); // POSIX Sun=0 → 1..7 (DS3231 user-defined DOw)
    }

    _wire->beginTransmission(_addr);
    _wire->write(0x00);
    _wire->write(bin2bcd(static_cast<uint8_t>(sec)));
    _wire->write(bin2bcd(static_cast<uint8_t>(min)));
    _wire->write(bin2bcd(static_cast<uint8_t>(hour))); // 24h: bit6=0
    _wire->write(dowRtc);
    _wire->write(bin2bcd(static_cast<uint8_t>(mday)));
    _wire->write(static_cast<uint8_t>(bin2bcd(static_cast<uint8_t>(mon)) | 0x80)); // century = 2000–2099
    _wire->write(bin2bcd(yr2));
    if (_wire->endTransmission() != 0) {
        return false;
    }
    // Clear OSF in status register after successful set (datasheet-style recovery).
    uint8_t status = 0;
    if (readRegisters(0x0F, &status, 1)) {
        status &= static_cast<uint8_t>(~0x80);
        writeRegister(0x0F, status);
    }
    return true;
}
