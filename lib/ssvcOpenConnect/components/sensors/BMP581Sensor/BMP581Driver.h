/**
 *   SSVC Open Connect
 *
 *   BMP581 register-level driver.
 *
 *   Hardware-independent: talks to the sensor through the abstract
 *   `bmp581::Transport`, uses no Arduino/ESP-IDF headers and is therefore
 *   compiled both into the firmware and into the host (native) unit tests.
 *
 *   Why not the Adafruit/Bosch API: on the tested module its initialisation
 *   reports success while the sensor is left with `press_en = 0` and the power
 *   mode in standby, so no pressure conversion ever happens and the data
 *   registers keep the "frame empty" marker 0x7F. This driver writes the
 *   configuration itself, verifies every register with a retry loop (the
 *   read-back stays stale for ~10 ms after a write on this chip) and validates
 *   the returned frame instead of trusting it.
 **/

#ifndef BMP581_DRIVER_H
#define BMP581_DRIVER_H

#include <cstddef>
#include <cstdint>

namespace bmp581 {

// ---------------------------------------------------------------------------
// Register map (Bosch BMP581 datasheet, 6.2 "Register map")
// ---------------------------------------------------------------------------
constexpr uint8_t REG_CHIP_ID = 0x01;
constexpr uint8_t REG_REV_ID = 0x02;
constexpr uint8_t REG_INT_STATUS = 0x27;
constexpr uint8_t REG_STATUS = 0x28;
constexpr uint8_t REG_OSR_CONFIG = 0x36;
constexpr uint8_t REG_ODR_CONFIG = 0x37;
constexpr uint8_t REG_OSR_EFF = 0x38;
constexpr uint8_t REG_CMD = 0x7E;

/** First of the six data bytes: temperature XLSB..MSB, then pressure XLSB..MSB. */
constexpr uint8_t REG_DATA_START = 0x1D;
constexpr uint8_t DATA_FRAME_BYTES = 6;

constexpr uint8_t CMD_SOFT_RESET = 0xB6;
constexpr uint8_t CHIP_ID_BMP581 = 0x50;

// OSR_CONFIG (0x36) bits
constexpr uint8_t OSR_TEMP_MASK = 0x07;
constexpr uint8_t OSR_PRESS_MASK = 0x38;
constexpr uint8_t OSR_PRESS_SHIFT = 3;
constexpr uint8_t OSR_PRESS_EN = 0x40;

// ODR_CONFIG (0x37) bits
constexpr uint8_t ODR_POWER_MASK = 0x03;
constexpr uint8_t ODR_MASK = 0x7C;
constexpr uint8_t ODR_SHIFT = 2;

enum class PowerMode : uint8_t {
    STANDBY = 0x00,
    NORMAL = 0x01,
    FORCED = 0x02,
    CONTINUOUS = 0x03,
};

/** Oversampling codes (0 = disabled, 1 = 2x ... 6 = 64x, 7 = 128x). */
constexpr uint8_t OSR_NONE = 0x00;
constexpr uint8_t OSR_2X = 0x01;
constexpr uint8_t OSR_16X = 0x04;
constexpr uint8_t OSR_64X = 0x06;

/** Output data rate codes (BMP581 datasheet table). */
constexpr uint8_t ODR_240_HZ = 0x00;
constexpr uint8_t ODR_50_HZ = 0x0F;
constexpr uint8_t ODR_10_HZ = 0x17;
constexpr uint8_t ODR_1_HZ = 0x1C;

// ---------------------------------------------------------------------------
// Data frame markers
// ---------------------------------------------------------------------------
/** The chip uses 0x7F bytes in the data registers when no frame is available. */
constexpr uint32_t RAW_EMPTY_FRAME = 0x7F7F7Fu;
/** Datasheet marker for "no data" (bit 23 set). */
constexpr uint32_t RAW_INVALID = 0x800000u;

// ---------------------------------------------------------------------------
// Fixed point scales (datasheet: 1/64 Pa per LSB, 1/2^16 degC per LSB)
// ---------------------------------------------------------------------------
constexpr float PRESSURE_LSB_PA = 1.0f / 64.0f;
constexpr float TEMPERATURE_LSB_C = 1.0f / 65536.0f;
/** 1 hPa = 0.750061683 mmHg. */
constexpr float HPA_TO_MMHG = 0.750061683f;

// ---------------------------------------------------------------------------
// Pure helpers
// ---------------------------------------------------------------------------

/** Assembles a 24-bit value from the chip's LSB-first byte order. */
inline uint32_t raw24(const uint8_t* lsbFirst) {
    return static_cast<uint32_t>(lsbFirst[0]) |
           (static_cast<uint32_t>(lsbFirst[1]) << 8) |
           (static_cast<uint32_t>(lsbFirst[2]) << 16);
}

/** Sign-extends a 24-bit two's complement value. */
inline int32_t signExtend24(const uint32_t raw) {
    return (raw & 0x800000u) ? static_cast<int32_t>(raw) - 0x1000000 : static_cast<int32_t>(raw);
}

inline float pressureFromRaw(const uint32_t rawPressure) {
    return static_cast<float>(rawPressure) * PRESSURE_LSB_PA;  // Pa
}

inline float pressureHpa(const uint32_t rawPressure) {
    return pressureFromRaw(rawPressure) / 100.0f;
}

inline float temperatureC(const uint32_t rawTemperature) {
    return static_cast<float>(signExtend24(rawTemperature)) * TEMPERATURE_LSB_C;
}

inline float hpaToMmHg(const float hpa) {
    return hpa * HPA_TO_MMHG;
}

/**
 * Pressure is a 24-bit unsigned value: 0x7F7F7F means "no frame" and >= 0x800000
 * is the datasheet invalid marker. Without this check an empty frame is turned
 * into a plausible-looking but meaningless 1305.58 hPa, which is how the frozen
 * 979.266 mmHg appeared.
 */
inline bool isPressureRawValid(const uint32_t raw) {
    return raw != RAW_EMPTY_FRAME && raw < RAW_INVALID;
}

/**
 * Temperature is 24-bit two's complement, so every value below 0 degC has bit 23
 * set (0xFA8000 is -5.5 degC). Only the "no frame" marker is rejected here — the
 * pressure rule would discard the whole frame at any negative temperature.
 */
inline bool isTemperatureRawValid(const uint32_t raw) {
    return raw != RAW_EMPTY_FRAME;
}

struct Config {
    uint8_t osrConfig = 0;      ///< raw value of 0x36
    uint8_t odrConfig = 0;      ///< raw value of 0x37
    bool pressEnabled = false;  ///< 0x36 bit 6
    uint8_t osrTemp = OSR_NONE;
    uint8_t osrPress = OSR_NONE;
    uint8_t odr = ODR_240_HZ;
    PowerMode power = PowerMode::STANDBY;

    /** True when the pressure channel is actually enabled and oversampled. */
    bool pressureConfigured() const { return pressEnabled && osrPress != OSR_NONE; }
    /** True when the chip performs conversions on its own. */
    bool isRunning() const {
        return power == PowerMode::NORMAL || power == PowerMode::CONTINUOUS;
    }
    /** True when the configuration is the one this driver writes. */
    bool isUsable() const { return pressureConfigured() && isRunning(); }
};

inline Config decodeConfig(const uint8_t osrConfig, const uint8_t odrConfig) {
    Config cfg;
    cfg.osrConfig = osrConfig;
    cfg.odrConfig = odrConfig;
    cfg.pressEnabled = (osrConfig & OSR_PRESS_EN) != 0;
    cfg.osrTemp = static_cast<uint8_t>(osrConfig & OSR_TEMP_MASK);
    cfg.osrPress = static_cast<uint8_t>((osrConfig & OSR_PRESS_MASK) >> OSR_PRESS_SHIFT);
    cfg.odr = static_cast<uint8_t>((odrConfig & ODR_MASK) >> ODR_SHIFT);
    cfg.power = static_cast<PowerMode>(odrConfig & ODR_POWER_MASK);
    return cfg;
}

inline uint8_t makeOsrConfig(const uint8_t osrTemp, const uint8_t osrPress, const bool pressEnabled) {
    return static_cast<uint8_t>(((osrPress << OSR_PRESS_SHIFT) & OSR_PRESS_MASK) |
                                (osrTemp & OSR_TEMP_MASK) | (pressEnabled ? OSR_PRESS_EN : 0));
}

inline uint8_t makeOdrConfig(const uint8_t odr, const PowerMode power) {
    return static_cast<uint8_t>(((odr << ODR_SHIFT) & ODR_MASK) |
                                (static_cast<uint8_t>(power) & ODR_POWER_MASK));
}

// ---------------------------------------------------------------------------
// Transport abstraction
// ---------------------------------------------------------------------------

class Transport {
public:
    virtual ~Transport() = default;

    virtual bool writeReg(uint8_t reg, uint8_t value) = 0;
    virtual bool readRegs(uint8_t reg, uint8_t* dst, size_t len) = 0;
    virtual void delayMs(uint32_t ms) = 0;
};

// ---------------------------------------------------------------------------
// Driver
// ---------------------------------------------------------------------------

struct Frame {
    bool valid = false;
    uint32_t rawPressure = 0;
    uint32_t rawTemperature = 0;
    float pressureHpa = 0.0f;
    float temperatureC = 0.0f;
};

/** Coarse error class, so the caller can map it to the right hardware fault. */
enum class Error : uint8_t {
    NONE = 0,
    BUS,       ///< an I2C transaction failed
    CHIP_ID,   ///< unexpected / unreadable chip id
    CONFIG,    ///< the sensor did not accept the configuration
    NO_DATA,   ///< the sensor returns no measurement frame
};

class Driver {
public:
    explicit Driver(Transport& transport) : _transport(transport) {}

    // --- tuning (defaults match the measured hardware behaviour) ---
    uint32_t resetSettleMs = 50;   ///< NVM load after a soft reset
    uint32_t verifyDelayMs = 15;   ///< read-back is stale for ~10 ms after a write
    uint32_t reconfigureDelayMs = 50;  ///< at least one conversion after reconfiguring
    uint8_t verifyTries = 5;

    /**
     * Reads the chip id, performs a soft reset and programs a verified
     * configuration (pressure enabled, 16x/16x OSR, 50 Hz, normal mode).
     */
    bool begin() {
        _chipId = 0;
        _lastError = nullptr;
        _lastErrorCode = Error::NONE;

        uint8_t id = 0;
        if (!readRegister(REG_CHIP_ID, id)) {
            return fail(Error::BUS, "chip id read failed (I2C)");
        }
        _chipId = id;
        if (id != CHIP_ID_BMP581) {
            return fail(Error::CHIP_ID, "unexpected chip id");
        }

        if (!_transport.writeReg(REG_CMD, CMD_SOFT_RESET)) {
            return fail(Error::BUS, "soft reset write failed (I2C)");
        }
        _transport.delayMs(resetSettleMs);

        return configure();
    }

    /** Programs and verifies OSR_CONFIG + ODR_CONFIG. */
    bool configure() {

        const uint8_t osr = makeOsrConfig(OSR_16X, OSR_16X, true);
        if (!writeVerified(REG_OSR_CONFIG, osr)) {
            return fail(Error::CONFIG, "OSR_CONFIG (press_en) was not accepted by the sensor");
        }

        const uint8_t odr = makeOdrConfig(ODR_50_HZ, PowerMode::NORMAL);
        if (!writeVerified(REG_ODR_CONFIG, odr)) {
            return fail(Error::CONFIG, "ODR_CONFIG (normal mode) was not accepted by the sensor");
        }

        const Config cfg = readConfig();
        if (!cfg.pressureConfigured()) {
            return fail(Error::CONFIG, "pressure channel is still disabled after configuration");
        }
        if (!cfg.isRunning()) {
            return fail(Error::CONFIG, "sensor is still not in normal mode after configuration");
        }

        _lastError = nullptr;
        _lastErrorCode = Error::NONE;
        return true;
    }

    /**
     * Reads one measurement.
     *
     * An empty/invalid frame triggers a full reconfigure (the chip can silently
     * fall back to standby) and one more attempt, so a single glitch does not
     * poison the published value.
     */
    bool read(Frame& out) {
        Frame frame;
        if (!readFrame(frame)) {
            return fail(Error::BUS, "data frame read failed (I2C)");
        }

        if (!frame.valid) {
            ++_invalidFrames;
            // The chip can silently drop out of normal mode (or never accept the
            // configuration at all): reprogram it and give it one conversion.
            if (!configure()) {
                return false;  // lastError is set by configure()
            }
            ++_reconfigures;
            _transport.delayMs(reconfigureDelayMs);
            if (!readFrame(frame) || !frame.valid) {
                ++_invalidFrames;
                return fail(Error::NO_DATA, "no valid data frame after reconfiguration");
            }
        }

        out = frame;
        _lastError = nullptr;
        _lastErrorCode = Error::NONE;
        return true;
    }

    /** Reads and decodes the configuration registers. */
    Config readConfig() {
        uint8_t osr = 0;
        uint8_t odr = 0;
        const bool osrOk = readRegister(REG_OSR_CONFIG, osr);
        const bool odrOk = readRegister(REG_ODR_CONFIG, odr);
        if (!osrOk || !odrOk) {
            _lastError = "configuration read failed (I2C)";
            _lastErrorCode = Error::BUS;
        }
        return decodeConfig(osr, odr);
    }

    bool readRegister(const uint8_t reg, uint8_t& value) {
        uint8_t buffer = 0;
        if (!_transport.readRegs(reg, &buffer, 1)) {
            return false;
        }
        value = buffer;
        return true;
    }

    uint8_t chipId() const { return _chipId; }
    const char* lastError() const { return _lastError ? _lastError : ""; }
    Error lastErrorCode() const { return _lastErrorCode; }
    uint32_t writeAttempts() const { return _writeAttempts; }
    uint32_t invalidFrames() const { return _invalidFrames; }
    uint32_t reconfigures() const { return _reconfigures; }

private:
    bool fail(const Error code, const char* message) {
        _lastErrorCode = code;
        _lastError = message;
        return false;
    }

    /** Writes a register and verifies it, allowing for a stale read-back. */
    bool writeVerified(const uint8_t reg, const uint8_t value) {
        for (uint8_t attempt = 0; attempt < verifyTries; ++attempt) {
            ++_writeAttempts;
            if (!_transport.writeReg(reg, value)) {
                _transport.delayMs(verifyDelayMs);
                continue;  // bus error: retry
            }
            _transport.delayMs(verifyDelayMs);
            uint8_t readBack = 0;
            if (readRegister(reg, readBack) && readBack == value) {
                return true;
            }
        }
        return false;
    }

    /** Reads and validates the six data bytes. */
    bool readFrame(Frame& frame) {
        uint8_t bytes[DATA_FRAME_BYTES] = {0};
        if (!_transport.readRegs(REG_DATA_START, bytes, DATA_FRAME_BYTES)) {
            return false;
        }

        frame.rawTemperature = raw24(bytes);
        frame.rawPressure = raw24(bytes + 3);
        frame.valid = isPressureRawValid(frame.rawPressure) && isTemperatureRawValid(frame.rawTemperature);
        frame.pressureHpa = frame.valid ? pressureHpa(frame.rawPressure) : 0.0f;
        frame.temperatureC = frame.valid ? temperatureC(frame.rawTemperature) : 0.0f;
        return true;
    }

    Transport& _transport;
    uint8_t _chipId = 0;
    const char* _lastError = nullptr;
    Error _lastErrorCode = Error::NONE;
    uint32_t _writeAttempts = 0;
    uint32_t _invalidFrames = 0;
    uint32_t _reconfigures = 0;
};

}  // namespace bmp581

#endif  // BMP581_DRIVER_H
