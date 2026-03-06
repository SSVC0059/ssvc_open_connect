#pragma once
#include "core/SubsystemManager/SubsystemManager.h"
#include "components/sensors/BMP581Sensor/BMP581Sensor.h"

class AtmosphericSubsystem final : public Subsystem {
public:
    AtmosphericSubsystem() = default;

    ~AtmosphericSubsystem() override = default; // Важно для vtable

    static std::string getName() { return "atm_sensor"; }

    void initialize() override;
    void enable() override;
    void disable() override;

private:
    BMP581Sensor* _sensor = nullptr;
    bool _initialized = false;
    bool _enabled = false;

    static constexpr auto TAG = "AtmosphericSubsystem";
};