// Native unit tests for the BMP581 register driver.
//
// The driver is hardware-independent (bmp581::Transport), so these tests run on
// the host PC against a fake sensor that models the behaviour measured on the
// real module:
//   * configuration writes can be dropped (the original firmware bug),
//   * a register read-back stays stale for ~10 ms after a write,
//   * the data registers contain 0x7F7F7F ("no frame") when the chip is not
//     converting, which used to be published as a frozen 979.266 mmHg.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#ifdef ERROR
#undef ERROR
#endif

#include <cmath>
#include <cstring>

#include "components/sensors/BMP581Sensor/BMP581Driver.h"

namespace {

constexpr uint8_t kTempLsb = 0x1D;
constexpr uint8_t kPressLsb = 0x20;

class FakeBmp581 final : public bmp581::Transport {
public:
    static constexpr uint8_t kRegCount = 0x80;

    uint8_t regs[kRegCount] = {};
    uint8_t visible[kRegCount] = {};
    uint32_t nowMs = 0;
    uint32_t staleMs = 8;  ///< real chip: read-back lags ~10 ms behind a write

    int ignoreConfigWrites = 0;  ///< drop the next N writes to 0x36/0x37
    bool ignoreAllConfigForever = false;
    bool dataAlwaysEmpty = false;  ///< chip answers but never produces a frame
    bool resetObserved = false;

    float pressurePa = 102398.0f;
    float temperatureC = 26.0f;

    int writes = 0;
    int reads = 0;

    FakeBmp581() { powerOn(); }

    // --- bmp581::Transport -------------------------------------------------
    bool writeReg(const uint8_t reg, const uint8_t value) override {
        ++writes;
        if (busDown) return false;

        if (reg == bmp581::REG_CMD && value == bmp581::CMD_SOFT_RESET) {
            resetObserved = true;
            powerOn();
            return true;
        }

        if (reg == bmp581::REG_OSR_CONFIG || reg == bmp581::REG_ODR_CONFIG) {
            if (ignoreAllConfigForever) return true;  // ack, but nothing happens
            if (ignoreConfigWrites > 0) {
                --ignoreConfigWrites;
                return true;  // ack, but the write is lost (the real bug)
            }
        }

        if (reg == bmp581::REG_ODR_CONFIG) {
            regs[reg] = value;
            if ((value & bmp581::ODR_POWER_MASK) ==
                static_cast<uint8_t>(bmp581::PowerMode::FORCED)) {
                // One conversion, then the chip returns to standby on its own.
                produceFrame();
                regs[bmp581::REG_ODR_CONFIG] =
                    static_cast<uint8_t>(value & ~bmp581::ODR_POWER_MASK);
            }
        } else {
            regs[reg] = value;
        }

        markDirty();
        return true;
    }

    bool readRegs(const uint8_t reg, uint8_t* dst, const size_t len) override {
        ++reads;
        if (busDown) return false;
        for (size_t i = 0; i < len; ++i) {
            const uint8_t addr = static_cast<uint8_t>(reg + i);
            dst[i] = addr < kRegCount ? visible[addr] : 0xFF;
        }
        return true;
    }

    void delayMs(const uint32_t ms) override {
        nowMs += ms;
        commit();
    }

    // --- knobs -------------------------------------------------------------
    bool busDown = false;

    /** Empties the data registers so the next reads return the "no frame" marker. */
    void makeDataEmpty() {
        dataAlwaysEmpty = true;
        fillEmptyFrame();
        publishNow();
    }

    /** Pretends the chip dropped out of normal mode (silently). */
    void dropToStandby() {
        regs[bmp581::REG_ODR_CONFIG] =
            static_cast<uint8_t>(regs[bmp581::REG_ODR_CONFIG] & ~bmp581::ODR_POWER_MASK);
        fillEmptyFrame();
        publishNow();
    }

    bmp581::Config config() const {
        return bmp581::decodeConfig(regs[bmp581::REG_OSR_CONFIG], regs[bmp581::REG_ODR_CONFIG]);
    }

private:
    uint32_t commitAtMs = 0;

    void powerOn() {
        std::memset(regs, 0, sizeof(regs));
        regs[bmp581::REG_CHIP_ID] = bmp581::CHIP_ID_BMP581;
        regs[bmp581::REG_REV_ID] = 0x32;
        regs[bmp581::REG_INT_STATUS] = 0x10;  // "POR/softreset complete"
        regs[bmp581::REG_STATUS] = 0x02;
        regs[bmp581::REG_OSR_CONFIG] = 0x00;
        regs[bmp581::REG_ODR_CONFIG] = 0x70;  // standby, odr index 28 (1 Hz)
        regs[bmp581::REG_OSR_EFF] = 0x80;
        fillEmptyFrame();
        std::memcpy(visible, regs, sizeof(regs));
        commitAtMs = 0;
        dataAlwaysEmpty = false;
    }

    void fillEmptyFrame() {
        for (uint8_t r = kTempLsb; r <= kPressLsb + 2; ++r) {
            regs[r] = 0x7F;
        }
    }

    void produceFrame() {
        if (dataAlwaysEmpty) {
            fillEmptyFrame();
            return;
        }
        const float pa = pressurePa;
        const float tc = temperatureC;

        const int32_t tempRaw = static_cast<int32_t>(std::lround(tc * 65536.0f));
        const uint32_t pressRaw = static_cast<uint32_t>(std::lround(pa * 64.0f));

        for (int i = 0; i < 3; ++i) {
            regs[kTempLsb + i] = static_cast<uint8_t>((static_cast<uint32_t>(tempRaw) >> (8 * i)) & 0xFF);
            regs[kPressLsb + i] = static_cast<uint8_t>((pressRaw >> (8 * i)) & 0xFF);
        }
    }

    void markDirty() {
        commitAtMs = nowMs + staleMs;
        commit();
    }

    /** Makes a hardware change visible immediately (no stale window). */
    void publishNow() {
        commitAtMs = 0;
        commit();
    }

    void commit() {
        if (nowMs < commitAtMs) return;
        const bmp581::Config cfg = config();
        if (cfg.isUsable()) {
            produceFrame();
        } else if (!dataAlwaysEmpty) {
            fillEmptyFrame();
        }
        std::memcpy(visible, regs, sizeof(regs));
    }
};

}  // namespace

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------

TEST_CASE("driver programs press_en and normal mode") {
    FakeBmp581 fake;
    bmp581::Driver driver(fake);

    // Default state of a real sensor after power-up.
    CHECK_FALSE(fake.config().pressEnabled);
    CHECK(fake.config().power == bmp581::PowerMode::STANDBY);

    CHECK(driver.begin());
    CHECK(fake.resetObserved);

    const bmp581::Config cfg = driver.readConfig();
    CHECK(cfg.pressEnabled);
    CHECK(cfg.osrPress == bmp581::OSR_16X);
    CHECK(cfg.osrTemp == bmp581::OSR_16X);
    CHECK(cfg.odr == bmp581::ODR_50_HZ);
    CHECK(cfg.power == bmp581::PowerMode::NORMAL);
    CHECK(cfg.isUsable());
}

TEST_CASE("driver retries while the sensor ignores configuration writes") {
    FakeBmp581 fake;
    fake.ignoreConfigWrites = 3;  // the real module sometimes loses these writes
    bmp581::Driver driver(fake);

    CHECK(driver.begin());
    CHECK(fake.config().isUsable());
    CHECK(driver.writeAttempts() > 2);
}

TEST_CASE("driver gives up when the configuration never sticks") {
    FakeBmp581 fake;
    fake.ignoreAllConfigForever = true;
    bmp581::Driver driver(fake);

    CHECK_FALSE(driver.begin());
    CHECK(driver.lastErrorCode() == bmp581::Error::CONFIG);
    CHECK(fake.config().pressEnabled == false);
}

TEST_CASE("chip id mismatch is reported") {
    FakeBmp581 fake;
    fake.regs[bmp581::REG_CHIP_ID] = 0x58;  // e.g. a BMP280 sitting on the bus
    std::memcpy(fake.visible, fake.regs, sizeof(fake.regs));

    bmp581::Driver driver(fake);
    CHECK_FALSE(driver.begin());
    CHECK(driver.lastErrorCode() == bmp581::Error::CHIP_ID);
}

TEST_CASE("bus failure during chip id read is reported") {
    FakeBmp581 fake;
    fake.busDown = true;

    bmp581::Driver driver(fake);
    CHECK_FALSE(driver.begin());
    CHECK(driver.lastErrorCode() == bmp581::Error::BUS);
}

TEST_CASE("stale read-back is tolerated by the verify delay") {
    FakeBmp581 fake;
    fake.staleMs = 8;

    bmp581::Driver driver(fake);
    driver.verifyDelayMs = 15;
    CHECK(driver.begin());

    // ... and the same register read-back without the delay never verifies.
    FakeBmp581 slowFake;
    slowFake.staleMs = 10;
    bmp581::Driver impatient(slowFake);
    impatient.verifyDelayMs = 0;
    CHECK_FALSE(impatient.begin());
    CHECK(impatient.lastErrorCode() == bmp581::Error::CONFIG);
}

// ---------------------------------------------------------------------------
// Data frames
// ---------------------------------------------------------------------------

TEST_CASE("a configured sensor returns a valid measurement") {
    FakeBmp581 fake;
    fake.pressurePa = 102398.0f;
    fake.temperatureC = 26.0f;

    bmp581::Driver driver(fake);
    REQUIRE(driver.begin());

    bmp581::Frame frame;
    REQUIRE(driver.read(frame));
    CHECK(frame.valid);
    CHECK(frame.pressureHpa == doctest::Approx(1023.98f).epsilon(0.001));
    CHECK(frame.temperatureC == doctest::Approx(26.0f).epsilon(0.001));
    CHECK(frame.rawPressure == static_cast<uint32_t>(std::lround(102398.0f * 64.0f)));
}

TEST_CASE("the empty frame marker is never published as a pressure") {
    // This is the original bug: 0x7F7F7F decodes to a plausible-looking
    // 1305.58 hPa and was frozen into 979.266 mmHg.
    CHECK(bmp581::pressureHpa(bmp581::RAW_EMPTY_FRAME) == doctest::Approx(1305.58f).epsilon(0.001));
    CHECK(bmp581::hpaToMmHg(bmp581::pressureHpa(bmp581::RAW_EMPTY_FRAME)) ==
          doctest::Approx(979.266f).epsilon(0.001));
    CHECK_FALSE(bmp581::isPressureRawValid(bmp581::RAW_EMPTY_FRAME));

    FakeBmp581 fake;
    bmp581::Driver driver(fake);
    REQUIRE(driver.begin());

    fake.makeDataEmpty();  // chip answers, but never completes a conversion

    bmp581::Frame frame;
    CHECK_FALSE(driver.read(frame));
    CHECK_FALSE(frame.valid);
    CHECK(frame.pressureHpa == doctest::Approx(0.0f));
    CHECK(driver.lastErrorCode() == bmp581::Error::NO_DATA);
    CHECK(driver.invalidFrames() > 0);
}

TEST_CASE("the datasheet invalid marker is rejected") {
    CHECK_FALSE(bmp581::isPressureRawValid(bmp581::RAW_INVALID));
    CHECK_FALSE(bmp581::isPressureRawValid(0xFFFFFFu));
    CHECK(bmp581::isPressureRawValid(0x63FF83u));
}

TEST_CASE("negative temperature does not invalidate the frame") {
    // Температура BMP581 — 24-битное дополнение до двух: у -5.5 °C (raw 0xFA8000)
    // старший бит установлен. При проверке «как давление» такой кадр отбраковывался
    // и датчик отдавал NO_DATA при любой отрицательной температуре.
    FakeBmp581 fake;
    fake.pressurePa = 102398.0f;
    fake.temperatureC = -5.5f;

    bmp581::Driver driver(fake);
    REQUIRE(driver.begin());

    bmp581::Frame frame;
    REQUIRE(driver.read(frame));
    CHECK(frame.valid);
    CHECK(frame.rawTemperature >= bmp581::RAW_INVALID);  // действительно отрицательная
    CHECK(frame.temperatureC == doctest::Approx(-5.5f).epsilon(0.001));
    CHECK(frame.pressureHpa == doctest::Approx(1023.98f).epsilon(0.001));
    CHECK(driver.invalidFrames() == 0);
}

TEST_CASE("temperature validity accepts negatives but rejects the empty marker") {
    CHECK(bmp581::isTemperatureRawValid(0xFA8000u));  // -5.5 C
    CHECK(bmp581::isTemperatureRawValid(0x000000u));  // 0 C
    CHECK(bmp581::isTemperatureRawValid(0x190000u));  // +25 C
    CHECK_FALSE(bmp581::isTemperatureRawValid(bmp581::RAW_EMPTY_FRAME));
}

TEST_CASE("an empty frame is recovered by reconfiguring the sensor") {
    FakeBmp581 fake;
    bmp581::Driver driver(fake);
    REQUIRE(driver.begin());

    fake.dropToStandby();  // the chip silently fell back to standby

    bmp581::Frame frame;
    REQUIRE(driver.read(frame));
    CHECK(frame.valid);
    CHECK(driver.reconfigures() == 1);
    CHECK(fake.config().isUsable());
}

// ---------------------------------------------------------------------------
// Decoding of real data captured on the device
// ---------------------------------------------------------------------------

TEST_CASE("frames captured from the real sensor decode to sane values") {
    // 56 13 1A 83 FF 63 -> 26.076 C, 1023.98 hPa (captured while the sensor was
    // correctly configured by hand).
    const uint8_t bytes[] = {0x56, 0x13, 0x1A, 0x83, 0xFF, 0x63};
    const uint32_t rawTemperature = bmp581::raw24(bytes);
    const uint32_t rawPressure = bmp581::raw24(bytes + 3);

    CHECK(bmp581::isTemperatureRawValid(rawTemperature));
    CHECK(bmp581::isPressureRawValid(rawPressure));
    CHECK(bmp581::temperatureC(rawTemperature) == doctest::Approx(26.076f).epsilon(0.001));
    CHECK(bmp581::pressureHpa(rawPressure) == doctest::Approx(1023.98f).epsilon(0.001));
    CHECK(bmp581::hpaToMmHg(1023.98f) == doctest::Approx(767.5f).epsilon(0.01));
}

TEST_CASE("negative temperatures are sign extended") {
    // -5.5 C -> raw = -5.5 * 65536 = -360448 = 0xFA8000 (24 bit two's complement)
    const uint8_t bytes[] = {0x00, 0x80, 0xFA};
    CHECK(bmp581::temperatureC(bmp581::raw24(bytes)) == doctest::Approx(-5.5f).epsilon(0.001));
}

// ---------------------------------------------------------------------------
// Register encoding
// ---------------------------------------------------------------------------

TEST_CASE("the broken configuration from the device log is detected as unusable") {
    // osr=0x00 (press_en=0), odr=0xF0 (standby) - exactly what the firmware
    // reported before the fix.
    const bmp581::Config cfg = bmp581::decodeConfig(0x00, 0xF0);
    CHECK_FALSE(cfg.pressEnabled);
    CHECK(cfg.power == bmp581::PowerMode::STANDBY);
    CHECK_FALSE(cfg.pressureConfigured());
    CHECK_FALSE(cfg.isRunning());
    CHECK_FALSE(cfg.isUsable());
}

TEST_CASE("register encoding round-trips") {
    const uint8_t osr = bmp581::makeOsrConfig(bmp581::OSR_16X, bmp581::OSR_16X, true);
    CHECK(osr == 0x64);

    const uint8_t odr = bmp581::makeOdrConfig(bmp581::ODR_50_HZ, bmp581::PowerMode::NORMAL);
    CHECK(odr == 0x3D);

    const bmp581::Config cfg = bmp581::decodeConfig(osr, odr);
    CHECK(cfg.pressEnabled);
    CHECK(cfg.osrPress == bmp581::OSR_16X);
    CHECK(cfg.osrTemp == bmp581::OSR_16X);
    CHECK(cfg.odr == bmp581::ODR_50_HZ);
    CHECK(cfg.power == bmp581::PowerMode::NORMAL);
}

TEST_CASE("mmHg conversion matches the reference value") {
    CHECK(bmp581::hpaToMmHg(1013.25f) == doctest::Approx(760.0f).epsilon(0.001));
    CHECK(bmp581::hpaToMmHg(1000.0f) == doctest::Approx(750.06f).epsilon(0.001));
}
