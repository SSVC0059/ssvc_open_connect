#ifndef SUBSYSTEM_MANAGER_H
#define SUBSYSTEM_MANAGER_H

#include <memory>
#include <unordered_map>
#include <vector>
#include <functional>
#include <ArduinoJson.h>
#include <core/OpenConnectSettings/OpenConnectSettings.h>
#include <core/OpenConnectSettings/GlobalConfig.h>

class OpenConnectSettingsService;

// Базовый класс для всех подсистем
class Subsystem {
public:
    virtual ~Subsystem() = default;
    virtual void initialize() = 0;
    virtual void enable() = 0;
    virtual void disable() = 0;
};

// Структура для хранения информации о подсистеме
struct SubsystemInfo {
    bool enabled;
    std::shared_ptr<Subsystem> instance;
};


class SubsystemManager {
public:
    using SubsystemFactory = std::function<std::shared_ptr<Subsystem>()>;
    
    static SubsystemManager& instance();
    std::shared_ptr<Subsystem> getSubsystem(const std::string& name) const;

    void begin();
    
    template<typename T>
    void registerSubsystem() {
        static_assert(std::is_base_of_v<Subsystem, T>,
                     "T must inherit from Subsystem");

        const std::string name = T::getName();
        ESP_LOGD(TAG, "[REGISTER] Registering subsystem: %s", name.c_str());

        if (_subsystemFactories.count(name)) {
            ESP_LOGW(TAG, "[REGISTER] %s already registered!", name.c_str());
            return;
        }

        _subsystemFactories[name] = []() {
            ESP_LOGD(TAG, "[CREATE] Creating instance of %s", T::getName().c_str());
            return std::make_shared<T>();
        };

        ESP_LOGI(TAG, "[REGISTER] %s registered successfully", name.c_str());
    }
    
    void setInitialState(const std::string& name, bool enabled);
    
    bool enableSubsystem(const std::string& name);
    bool disableSubsystem(const std::string& name);
    bool exists(const std::string& name) const;
    
    std::vector<std::string> getAvailableSubsystems() const;
    std::vector<std::string> getEnabledSubsystems() const;
    std::unordered_map<std::string, bool> getSubsystemsStatus() const;
    bool isSubsystemEnabled(const std::string& name) const;


private:
    SubsystemManager();
    void loadConfig();
    void saveConfig() const;
    bool isSubsystemRegistered(const std::string& name) const;

    std::unordered_map<std::string, SubsystemFactory> _subsystemFactories;
    std::unordered_map<std::string, bool> _initialStates;
    std::unordered_map<std::string, SubsystemInfo> _subsystems;


    bool _initialized = false;
    
    static SubsystemManager* _instance;
    static constexpr const char* TAG = "SubsystemManager";

};

#endif // SUBSYSTEM_MANAGER_H