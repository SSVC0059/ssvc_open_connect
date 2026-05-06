#ifndef LCD1602_SUBSYSTEM_H
#define LCD1602_SUBSYSTEM_H

#include "core/SubsystemManager/SubsystemManager.h"

#include <cstdint>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

class I2CBusSubsystem;

class Lcd1602Subsystem final : public Subsystem {
public:
    static std::string getName() { return "lcd1602_display"; }

    void initialize() override;
    void enable() override;
    void disable() override;

private:
    static void workerTaskEntry(void* arg);
    void workerLoop();
    void writeStatusPage(uint32_t pageIndex);
    void clearDisplay();

    bool _initialized = false;
    bool _enabled = false;
    I2CBusSubsystem* _bus = nullptr;
    uint8_t _addr = 0x27;
    TaskHandle_t _worker = nullptr;

    static constexpr const char* TAG = "Lcd1602Subsystem";
};

#endif // LCD1602_SUBSYSTEM_H
