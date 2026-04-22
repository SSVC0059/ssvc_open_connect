#include "OpenConnectHardwareSettingsService.h"

#include "OpenConnectHardwareMigration.h"

#include <esp_log.h>

namespace {

constexpr const char* TAG = "OcHwSettings";

} // namespace

OpenConnectHardwareSettingsService::OpenConnectHardwareSettingsService() : StatefulService<OpenConnectHardwareConfig>() {}

OpenConnectHardwareSettingsService& OpenConnectHardwareSettingsService::instance() {
    static OpenConnectHardwareSettingsService inst;
    return inst;
}

void OpenConnectHardwareSettingsService::begin(FS* fs) {
    if (_begun || !fs) {
        return;
    }

    OpenConnectHardwareMigration::ensure(fs);

    _fsPersistence.reset(new FSPersistence<OpenConnectHardwareConfig>(
        [](OpenConnectHardwareConfig& s, JsonObject& root) { OpenConnectHardwareConfig::read(s, root); },
        [](JsonObject& root, OpenConnectHardwareConfig& s, const String& originId) {
            return OpenConnectHardwareConfig::update(root, s, originId);
        },
        this,
        fs,
        FILE_PATH));

    _fsPersistence->readFromFS();

    const StateUpdateResult cleared = update(
        [&](OpenConnectHardwareConfig& s) {
            if (!s.pendingReboot) {
                return StateUpdateResult::UNCHANGED;
            }
            s.pendingReboot = false;
            return StateUpdateResult::CHANGED;
        },
        "boot");

    if (cleared == StateUpdateResult::CHANGED) {
        ESP_LOGI(TAG, "Cleared pendingReboot after boot");
    }

    _begun = true;
}
