#ifndef VK_BOT_SUBSYSTEM_H
#define VK_BOT_SUBSYSTEM_H

#include "core/SubsystemManager/SubsystemManager.h"

class VkMessengerClient;

class VkBotSubsystem final : public Subsystem {
public:
    static std::string getName() { return "vk_bot"; }

    void initialize() override;
    void enable() override;
    void disable() override;

private:
    VkMessengerClient* _client = nullptr;
    bool _initialized = false;
    bool _enabled = false;
};

#endif
