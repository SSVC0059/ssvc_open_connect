#ifndef VKSETTINGSSERVICE_H
#define VKSETTINGSSERVICE_H

#include <StatefulService.h>
#include <FSPersistence.h>
#include "core/GlobalConfig/GlobalConfig.h"
#include <ESP32SvelteKit.h>
#include "core/profiles/IProfileObserver.h"

class AsyncWebServer;

struct VkSettings {
    String accessToken;
    String apiVersion;
    /** Numeric group id (positive) for wall.post owner_id = -group_id */
    String groupId;

    bool liveEnabled = true;
    String livePeerId;

    bool alertsEnabled = false;
    String alertsPeerId;

    bool summaryEnabled = false;
    String summaryPeerId;

    bool wallPostEnabled = false;
    /** Base URL for keyboard open_link (e.g. http://192.168.0.10); empty = use STA IP */
    String keyboardBaseUrl;
};

StateUpdateResult updateVkSettings(JsonObject& root, VkSettings& settings);
void readVkSettings(VkSettings& settings, JsonObject& root);

class VkSettingsService : public StatefulService<VkSettings>, public IProfileObserver {
public:
    static VkSettingsService* getInstance();
    static void setInstance(VkSettingsService* instance);

    void begin();

    VkSettingsService(AsyncWebServer* server, ESP32SvelteKit* esp32sveltekit);

    const char* getProfileKey() const override;
    void onProfileApply(const JsonObject& profile) override;
    void onProfileSave(JsonObject& profile) override;

private:
    FSPersistence<VkSettings> _fsPersistence;
    ESP32SvelteKit* _esp32sveltekit = nullptr;

    static VkSettingsService* _instance;
};

#endif
