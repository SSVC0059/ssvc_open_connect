#ifndef SSVC_OPEN_CONNECT_PROFILEHANDLER_H
#define SSVC_OPEN_CONNECT_PROFILEHANDLER_H

#include "PsychicHttp.h"
#include "SecurityManager.h"
#include "core/profiles/ProfileService.h"

class ProfileHandler {
public:
    explicit ProfileHandler(ProfileService* profileService);

    static esp_err_t handleGetProfiles(PsychicRequest* request);
    static esp_err_t handleGetActiveProfile(PsychicRequest* request);
    static esp_err_t handleCreateProfile(PsychicRequest* request);
    static esp_err_t handleDeleteProfile(PsychicRequest* request);
    static esp_err_t handleCopyProfile(PsychicRequest* request);
    static esp_err_t handleUpdateProfileMeta(PsychicRequest* request);
    static esp_err_t handleSetActiveAndApplyProfile(PsychicRequest* request); // New combined handler
    static esp_err_t handleGetProfileContent(PsychicRequest* request);
    static esp_err_t handleSaveSettingsToProfile(PsychicRequest* request);
    static esp_err_t handleUpdateProfileContent(PsychicRequest* request);

private:
    ProfileService* _profileService;
};

#endif //SSVC_OPEN_CONNECT_PROFILEHANDLER_H
