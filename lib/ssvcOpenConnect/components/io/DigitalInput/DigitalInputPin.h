#ifndef SSVC_OPEN_CONNECT_DIGITAL_INPUT_PIN_H
#define SSVC_OPEN_CONNECT_DIGITAL_INPUT_PIN_H

#include <cstdint>
#include <string>

#include "components/io/Pcf8574/Pcf8574Input.h"

/**
 * Kind of a discrete input source. Mirrors Xkc25SensorConfig::kPinSource* (see
 * static_assert in Xkc25Sensor.cpp): 0 = native ESP32 GPIO, 1 = PCF8574 bit.
 */
namespace DigitalInputSourceKind {
constexpr uint8_t Gpio = 0;
constexpr uint8_t Pcf8574 = 1;
} // namespace DigitalInputSourceKind

/** Which physical input feeds a discrete sensor. */
struct DigitalInputSource {
    uint8_t source = DigitalInputSourceKind::Gpio;
    uint8_t gpio = 0;          // valid when source == Gpio
    uint8_t pcfAddress = 0x22; // 7-bit address of the input expander, when source == Pcf8574
    uint8_t pcfBit = 0;        // bit 0..7 on the expander, when source == Pcf8574

    bool isPcf() const { return source == DigitalInputSourceKind::Pcf8574; }

    /** Human-readable pin label: "GPIO 4" or "PCF8574 0x22 · P3". */
    std::string label() const;
};

/**
 * @brief A discrete input that is either a native GPIO or a bit on a PCF8574 expander.
 *
 * Centralises the difference so callers (XKC-25 sensors) read one level and never
 * branch on the transport. A failing I2C read is reported as ok == false, never
 * silently as level 0.
 */
class DigitalInputPin {
public:
    /**
     * @brief Arm the input.
     *
     * GPIO: pinMode(INPUT) — the pull is done by external wiring.
     * PCF8574: bind the I2C bus and write 0xFF once so every bit becomes an input.
     * @return false when the bus is inactive or the expander does not ACK.
     */
    bool begin(const DigitalInputSource& source);

    /** @return level (0/1); @p ok is false only when the transport failed (PCF). */
    int read(bool& ok) const;

    /** Internal pull-up/pull-down probing is only meaningful for a GPIO. */
    bool supportsPullProbe() const { return !_source.isPcf(); }

    const DigitalInputSource& source() const { return _source; }

    /** Human-readable pin label (same as DigitalInputSource::label). */
    const std::string& label() const { return _label; }

    /**
     * @brief One-shot read without arming (no write to the expander).
     *
     * For callers that only need the current level of an already-configured input
     * (e.g. the pre-start guard when the subsystem is not active).
     */
    static int readOnce(const DigitalInputSource& source, bool& ok);

private:
    DigitalInputSource _source{};
    std::string _label;
    /** setBus() is non-const and read() is const, so the reader must be mutable. */
    mutable Pcf8574Input _pcf{};
};

#endif // SSVC_OPEN_CONNECT_DIGITAL_INPUT_PIN_H
