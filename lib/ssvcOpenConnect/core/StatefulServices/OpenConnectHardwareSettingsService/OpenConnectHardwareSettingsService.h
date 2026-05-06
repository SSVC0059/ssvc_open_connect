#ifndef OPEN_CONNECT_HARDWARE_SETTINGS_SERVICE_H
#define OPEN_CONNECT_HARDWARE_SETTINGS_SERVICE_H

#include "OpenConnectHardwareConfig.h"

#include <FS.h>
#include <FSPersistence.h>
#include <memory>

class OpenConnectHardwareSettingsService final : public StatefulService<OpenConnectHardwareConfig> {
public:
    static OpenConnectHardwareSettingsService& instance();

    /** Must run before SubsystemManager::begin(); migrates NVS→JSON when needed. */
    void begin(FS* fs);

private:
    OpenConnectHardwareSettingsService();

    std::unique_ptr<FSPersistence<OpenConnectHardwareConfig>> _fsPersistence;
    bool _begun = false;

    static constexpr const char* FILE_PATH = "/config/oc_hardware.json";
};

#endif
