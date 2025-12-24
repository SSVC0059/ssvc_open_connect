#ifndef ProfileService_h
#define ProfileService_h

#include "IProfileObserver.h"
#include <vector>
#include <FS.h>
#include <ArduinoJson.h>
#include <set>

struct ProfileMetadata {
    String id;
    String name;
    String createdAt;
    bool isApplied;
};

class ProfileService {
public:
    static ProfileService* getInstance();
    void begin(FS* fs);

    void subscribe(IProfileObserver* observer);

    // Profile management methods
    std::vector<ProfileMetadata> getProfileList() const;
    String getActiveProfileId() const;
    String createProfile(const String& displayName);
    bool deleteProfile(const String& profileId) const;
    String copyProfile(const String& sourceProfileId, const String& newDisplayName) const;
    bool updateProfile(const String& profileId, const String& newDisplayName) const;
    bool setActiveAndApplyProfile(const String& profileId) const; // Combined method
    bool getProfileContent(const String& profileId, String& dest) const;
    void getProfileListAsJson(String& dest) const;
    bool saveCurrentSettingsToProfile(const String& profileId) const;

private:
    ProfileService();

    bool _createOrUpdateProfile(const String& profileId, const String& displayName) const;
    String _generateNewProfileId() const;

    static ProfileService* _instance;

    FS* _fs;
    const char* _profilesDir = "/profiles";
    const char* _metadataFilePath = "/profiles/profiles_metadata.json";
    std::vector<IProfileObserver*> _observers;

    static String getCurrentTimestamp();

    bool _readMetadata(JsonDocument& doc) const;
    bool _writeMetadata(const JsonDocument& doc) const;
    bool _getProfileMetadata(const String& profileId, JsonObject& profileObj) const;
    bool _profileFileExists(const String& profileId) const;
    bool _addProfileToMetadata(const String& profileId, const String& displayName) const; // Добавлено
    bool _getProfileDocument(const String& profileId, JsonDocument& doc) const; // Добавлено
    bool _writeProfileDocument(const String& profileId, const JsonDocument& doc) const; // Добавлено

    // Private helper methods for applying and setting active
    bool _applyProfileInternal(const String& profileId) const;
    bool _setActiveProfileInternal(const String& profileId) const;
};

#endif // ProfileService_h
