#ifndef SSVC_OPEN_CONNECT_PROFILEHANDLER_H
#define SSVC_OPEN_CONNECT_PROFILEHANDLER_H

#include <ESPAsyncWebServer.h>
#include "SecurityManager.h"
#include "core/profiles/ProfileService.h"

class ProfileHandler {
public:
    explicit ProfileHandler(ProfileService* profileService);

    static void handleGetProfiles(AsyncWebServerRequest* request);
    static void handleGetActiveProfile(AsyncWebServerRequest* request);
    static void handleCreateProfile(AsyncWebServerRequest* request, JsonVariant& json);
    static void handleDeleteProfile(AsyncWebServerRequest* request, JsonVariant& json);
    static void handleCopyProfile(AsyncWebServerRequest* request, JsonVariant& json);
    static void handleUpdateProfileMeta(AsyncWebServerRequest* request, JsonVariant& json);
    static void handleSetActiveAndApplyProfile(AsyncWebServerRequest* request, JsonVariant& json);
    static void handleGetProfileContent(AsyncWebServerRequest* request);
    static void handleSaveSettingsToProfile(AsyncWebServerRequest* request, JsonVariant& json);
    static void handleUpdateProfileContent(AsyncWebServerRequest* request, JsonVariant& json);

private:
    ProfileService* _profileService;
};

#endif //SSVC_OPEN_CONNECT_PROFILEHANDLER_H
