#ifndef SUBSYSTEM_MANAGER_H
#define SUBSYSTEM_MANAGER_H

#include <ArduinoJson.h>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>
#include <string>
#include <algorithm> // For std::find_if
#include <core/GlobalConfig/GlobalConfig.h>
#include <EventSocket.h>

class Subsystem {
public:
    virtual ~Subsystem() = default;
    virtual void initialize() = 0;
    virtual void enable() = 0;
    virtual void disable() = 0;
};

struct SubsystemInfo {
    std::string name;
    bool enabled;
    std::shared_ptr<Subsystem> instance;
    std::function<std::shared_ptr<Subsystem>()> factory;
};

class SubsystemManager {
public:
    static SubsystemManager& instance();
    std::shared_ptr<Subsystem> getSubsystem(const std::string& name) const;

    void begin();
    
    template<typename T>
    void registerSubsystem() {
        static_assert(std::is_base_of<Subsystem, T>::value, "T must inherit from Subsystem");
        const std::string name = T::getName();
        if (isSubsystemRegistered(name)) {
            ESP_LOGW(TAG, "[REGISTER] %s already registered!", name.c_str());
            return;
        }
        SubsystemInfo info;
        info.name = name;
        info.factory = []() { return std::make_shared<T>(); };
        _subsystems_info.push_back(std::move(info));
        ESP_LOGI(TAG, "[REGISTER] %s registered successfully", name.c_str());
    }

    template<typename T>
    void registerSingleton() {
        const std::string name = T::getName();
        if (isSubsystemRegistered(name)) {
            return;
        }
        SubsystemInfo info;
        info.name = name;
        info.factory = []() {
            return std::shared_ptr<T>(&T::getInstance(), [](T*){});
        };
        _subsystems_info.push_back(std::move(info));
        ESP_LOGI(TAG, "[REGISTER] %s registered as Singleton", name.c_str());
    }
    
    void setInitialState(const std::string& name, bool enabled);
    
    bool enableSubsystem(const std::string& name);
    bool disableSubsystem(const std::string& name);

    std::unordered_map<std::string, bool> getSubsystemsStatus() const;
    bool isSubsystemEnabled(const std::string& name) const;

private:
    SubsystemManager();
    void saveConfig() const;
    bool isSubsystemRegistered(const std::string& name) const;

    // Вектор для сохранения порядка регистрации
    std::vector<SubsystemInfo> _subsystems_info;
    // Карта для быстрых проверок начального состояния
    std::unordered_map<std::string, bool> _initialStates;

    bool _initialized = false;
    
    static SubsystemManager* _instance;
    static constexpr auto TAG = "SubsystemManager";
};

#endif // SUBSYSTEM_MANAGER_H