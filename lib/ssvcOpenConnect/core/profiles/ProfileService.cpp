#include "ProfileService.h"
#include <ctime>
#include <ArduinoJson.h>
#include <set>
#include <list>
#include "esp_log.h"

static const char* TAG = "ProfileService";

// Initialize the static instance pointer
ProfileService* ProfileService::_instance = nullptr;

// Static method to get the singleton instance
ProfileService* ProfileService::getInstance() {
    if (_instance == nullptr) {
        _instance = new ProfileService();
    }
    return _instance;
}

void ProfileService::begin(FS* fs) {
    _fs = fs;
    if (!_fs->exists(_profilesDir)) {
        _fs->mkdir(_profilesDir);
    }

    bool metadataModified = false;
    JsonDocument metadataDoc;
    bool metadataExists = _readMetadata(metadataDoc);

    if (!metadataExists || !metadataDoc.is<JsonArray>()) {
        metadataDoc.to<JsonArray>();
        metadataModified = true;
    }

    auto profiles = metadataDoc.as<JsonArray>();
    std::set<String> profileIdsInMetadata;
    std::list<int> indicesToRemove;

    for (int i = 0; i < profiles.size(); ++i) {
        auto id = profiles[i]["id"].as<String>();
        if (!_profileFileExists(id)) {
            indicesToRemove.push_front(i);
            metadataModified = true;
        } else {
            profileIdsInMetadata.insert(id);
        }
    }

    for (int index : indicesToRemove) {
        profiles.remove(index);
    }

    File profilesDir = _fs->open(_profilesDir);
    if (profilesDir) {
        File file = profilesDir.openNextFile();
        while (file) {
            String fileName = file.name();
            if (fileName.endsWith(".json") && fileName != "profiles_metadata.json") {
                String profileId = fileName.substring(0, fileName.lastIndexOf('.'));
                if (profileIdsInMetadata.find(profileId) == profileIdsInMetadata.end()) {
                    auto newProfile = profiles.add<JsonObject>();
                    newProfile["id"] = profileId;
                    newProfile["name"] = "Profile " + profileId;
                    newProfile["createdAt"] = getCurrentTimestamp();
                    newProfile["isApplied"] = false;
                    metadataModified = true;
                }
            }
            file = profilesDir.openNextFile();
        }
        profilesDir.close();
    }

    if (profiles.size() == 0) {
        const String defaultProfileId = "0";
        const String defaultProfileName = "Default Profile";
        if (_createOrUpdateProfile(defaultProfileId, defaultProfileName)) {
            // After creating the default profile, we need to re-read the metadata
            // and set the new profile as active.
            _readMetadata(metadataDoc); // Re-read into the main doc
            auto tempProfiles = metadataDoc.as<JsonArray>();
            for(JsonObject p : tempProfiles) {
                if (p["id"] == defaultProfileId) {
                    p["isApplied"] = true;
                    metadataModified = true; // Mark for writing at the end
                    break;
                }
            }
        }
    } else if (getActiveProfileId().isEmpty()) {
        profiles[0].as<JsonObject>()["isApplied"] = true;
        metadataModified = true;
    }

    if (metadataModified) {
        if (!_writeMetadata(metadataDoc)) {
            ESP_LOGE(TAG, "ERROR: Failed to update profiles metadata file!");
        }
    }
}

ProfileService::ProfileService() = default;

void ProfileService::subscribe(IProfileObserver* observer) {
    if (observer) {
        _observers.push_back(observer);
    }
}

String ProfileService::getCurrentTimestamp() {
    time_t now;
    time(&now);
    char buf[sizeof "2011-10-08T07:07:09Z"];
    strftime(buf, sizeof buf, "%Y-%m-%dT%H:%M:%SZ", gmtime(&now));
    return {buf};
}

// Helper methods for metadata management
bool ProfileService::_readMetadata(JsonDocument& doc) const {
    ESP_LOGI(TAG, "_readMetadata: Entry. Path=%s", _metadataFilePath); // Логирование входа
    File metadataFile = _fs->open(_metadataFilePath, "r");
    if (!metadataFile) {
        ESP_LOGE(TAG, "_readMetadata: Failed to open metadata file for reading: %s", _metadataFilePath); // Логирование ошибки
        return false;
    }
    ESP_LOGI(TAG, "_readMetadata: Metadata file opened: %s", _metadataFilePath); // Логирование открытия файла

    const DeserializationError error = deserializeJson(doc, metadataFile);
    metadataFile.close();
    if (error) {
        ESP_LOGE(TAG, "_readMetadata: Failed to deserialize metadata file %s: %s", _metadataFilePath, error.c_str()); // Логирование ошибки
        return false;
    }
    ESP_LOGI(TAG, "_readMetadata: Metadata deserialized successfully."); // Логирование десериализации
    return true;
}

bool ProfileService::_writeMetadata(const JsonDocument& doc) const {
    ESP_LOGI(TAG, "_writeMetadata: Entry. Path=%s", _metadataFilePath); // Логирование входа
    File metadataFile = _fs->open(_metadataFilePath, "w");
    if (!metadataFile) {
        ESP_LOGE(TAG, "_writeMetadata: Failed to open metadata file for writing: %s", _metadataFilePath); // Логирование ошибки
        return false;
    }
    ESP_LOGI(TAG, "_writeMetadata: Metadata file opened for writing: %s", _metadataFilePath); // Логирование открытия файла

    if (serializeJson(doc, metadataFile) == 0) {
        ESP_LOGE(TAG, "_writeMetadata: Failed to write metadata to file: %s", _metadataFilePath); // Логирование ошибки
        metadataFile.close();
        return false;
    }
    metadataFile.close();
    ESP_LOGI(TAG, "_writeMetadata: Metadata written successfully."); // Логирование записи
    return true;
}

bool ProfileService::_getProfileMetadata(const String& profileId, JsonObject& profileObj) const {
    JsonDocument doc; // Use JsonDocument
    if (!_readMetadata(doc)) {
        return false;
    }
    const JsonArray profiles = doc.as<JsonArray>();
    for (JsonObject profile : profiles) {
        if (profile["id"].as<String>() == profileId) {
            profileObj = profile;
            return true;
        }
    }
    return false;
}

bool ProfileService::_profileFileExists(const String& profileId) const {
    const String filePath = String(_profilesDir) + "/" + profileId + ".json";
    ESP_LOGI(TAG, "_profileFileExists: Checking path=%s", filePath.c_str()); // Логирование проверки пути
    const bool exists = _fs->exists(filePath);
    ESP_LOGI(TAG, "_profileFileExists: File %s exists: %s", filePath.c_str(), exists ? "true" : "false"); // Логирование результата
    return exists;
}

String ProfileService::_generateNewProfileId() const {
    ESP_LOGI(TAG, "_generateNewProfileId: Entry"); // Логирование входа
    JsonDocument metadataDoc;
    if (!_readMetadata(metadataDoc)) {
        ESP_LOGW(TAG, "_generateNewProfileId: Failed to read metadata, initializing new array."); // Логирование предупреждения
        metadataDoc.to<JsonArray>();
    }
    const auto profiles = metadataDoc.as<JsonArray>();

    int maxNumericId = -1;
    for (JsonObject profile : profiles) {
        auto idStr = profile["id"].as<String>();
        ESP_LOGD(TAG, "_generateNewProfileId: Processing profile ID: %s", idStr.c_str()); // Логирование каждого ID

        bool isNumeric = true;
        for (char c : idStr) {
            if (!isdigit(c)) {
                isNumeric = false;
                break;
            }
        }

        if (isNumeric) {
            int currentId = idStr.toInt();
            if (currentId > maxNumericId) {
                maxNumericId = currentId;
            }
        } else if (idStr.startsWith("profile")) {
            String numericPart = idStr.substring(7);
            bool isProfileNumeric = true;
            for (char c : numericPart) {
                if (!isdigit(c)) {
                    isProfileNumeric = false;
                    break;
                }
            }
            if (isProfileNumeric) {
                int currentId = numericPart.toInt();
                if (currentId > maxNumericId) {
                    maxNumericId = currentId;
                }
            }
        }
    }

    String newId = String(maxNumericId + 1);
    ESP_LOGI(TAG, "_generateNewProfileId: Generated new ID: %s", newId.c_str()); // Логирование сгенерированного ID
    return newId;
}

// Profile management methods
std::vector<ProfileMetadata> ProfileService::getProfileList() const {
    std::vector<ProfileMetadata> profiles;
    JsonDocument doc; // Use JsonDocument
    if (!_readMetadata(doc)) {
        return profiles;
    }

    const JsonArray metadataProfiles = doc.as<JsonArray>();
    for (JsonObject profile : metadataProfiles) {
        ProfileMetadata meta;
        meta.id = profile["id"].as<String>();
        meta.name = profile["name"].as<String>();
        meta.createdAt = profile["createdAt"].as<String>();
        meta.isApplied = profile["isApplied"].as<bool>();
        profiles.push_back(meta);
    }
    return profiles;
}

String ProfileService::getActiveProfileId() const {
    JsonDocument doc; // Use JsonDocument
    if (!_readMetadata(doc)) {
        return "";
    }
    const JsonArray profiles = doc.as<JsonArray>();
    for (JsonObject profile : profiles) {
        if (profile["isApplied"].as<bool>()) {
            return profile["id"].as<String>();
        }
    }
    return "";
}

bool ProfileService::_applyProfileInternal(const String& profileId) const {
    const String filePath = String(_profilesDir) + "/" + profileId + ".json";
    File profileFile = _fs->open(filePath, "r");
    if (!profileFile) {
        ESP_LOGE(TAG, "Profile file not found: %s", filePath.c_str());
        return false;
    }

    JsonDocument doc; // Use JsonDocument
    const DeserializationError error = deserializeJson(doc, profileFile);
    profileFile.close();
    if (error) {
        ESP_LOGE(TAG, "Failed to deserialize profile %s: %s", profileId.c_str(), error.c_str());
        return false;
    }

    const auto root = doc.as<JsonObject>();

    for (auto* observer : _observers) {
        const char* key = observer->getProfileKey();
        if (root[key].is<JsonObject>()) {
            observer->onProfileApply(root[key].as<JsonObject>());
        }
    }
    return true;
}

String ProfileService::createProfile(const String& displayName) {
    String newProfileId = _generateNewProfileId();
    if (_createOrUpdateProfile(newProfileId, displayName)) {
        return newProfileId;
    }
    return "";
}

bool ProfileService::_createOrUpdateProfile(const String& profileId, const String& displayName) const {
    JsonDocument profileDoc;
    auto profileRoot = profileDoc.to<JsonObject>();

    profileRoot["id"] = profileId;
    profileRoot["name"] = displayName;
    profileRoot["createdAt"] = getCurrentTimestamp();

    for (auto* observer : _observers) {
        const char* key = observer->getProfileKey();
        auto nestedObj = profileRoot[key].to<JsonObject>();
        observer->onProfileSave(nestedObj);
    }

    const String filePath = String(_profilesDir) + "/" + profileId + ".json";
    File profileFile = _fs->open(filePath, "w");
    if (!profileFile) {
        ESP_LOGE(TAG, "Failed to open profile file for writing: %s", filePath.c_str());
        return false;
    }
    if (serializeJson(profileDoc, profileFile) == 0) {
        ESP_LOGE(TAG, "Failed to write profile data to file: %s", filePath.c_str());
        profileFile.close();
        return false;
    }
    profileFile.close();

    // 2. Update metadata
    JsonDocument metadataDoc;
    if (!_readMetadata(metadataDoc)) {
        metadataDoc.to<JsonArray>();
    }

    auto profiles = metadataDoc.as<JsonArray>();
    bool profileExistsInMetadata = false;
    for (JsonObject profile : profiles) {
        if (profile["id"].as<String>() == profileId) {
            profile["name"] = displayName;
            profileExistsInMetadata = true;
            break;
        }
    }

    if (!profileExistsInMetadata) {
        auto newProfile = profiles.add<JsonObject>();
        newProfile["id"] = profileId;
        newProfile["name"] = displayName;
        newProfile["createdAt"] = getCurrentTimestamp();
        newProfile["isApplied"] = false; // New profiles are not active by default
    }

    return _writeMetadata(metadataDoc);
}

bool ProfileService::deleteProfile(const String& profileId) const {
    // 1. Delete the actual profile data file
    const String filePath = String(_profilesDir) + "/" + profileId + ".json";
    if (_fs->exists(filePath)) {
        if (!_fs->remove(filePath)) {
            ESP_LOGE(TAG, "Failed to delete profile file: %s", filePath.c_str());
            return false;
        }
    } else {
        ESP_LOGW(TAG, "Profile file not found for deletion: %s", filePath.c_str());
    }

    // 2. Remove from metadata
    JsonDocument metadataDoc; // Use JsonDocument
    if (!_readMetadata(metadataDoc)) {
        return false;
    }

    const JsonArray profiles = metadataDoc.as<JsonArray>();
    for (int i = 0; i < profiles.size(); ++i) {
        if (profiles[i]["id"].as<String>() == profileId) {
            profiles.remove(i);
            return _writeMetadata(metadataDoc);
        }
    }
    ESP_LOGW(TAG, "Profile ID '%s' not found in metadata for deletion.", profileId.c_str());
    return false;
}

// Новая вспомогательная функция для обновления метаданных после создания нового профиля
bool ProfileService::_addProfileToMetadata(const String& profileId, const String& displayName) const {
    JsonDocument metadataDoc;
    if (!_readMetadata(metadataDoc)) {
        ESP_LOGW(TAG, "_addProfileToMetadata: Failed to read metadata, initializing new array.");
        metadataDoc.to<JsonArray>();
    }

    auto profiles = metadataDoc.as<JsonArray>();
    auto newProfile = profiles.add<JsonObject>();
    newProfile["id"] = profileId;
    newProfile["name"] = displayName;
    newProfile["createdAt"] = getCurrentTimestamp();
    newProfile["isApplied"] = false;
    ESP_LOGI(TAG, "_addProfileToMetadata: New profile metadata added to array.");

    return _writeMetadata(metadataDoc);
}

String ProfileService::copyProfile(const String& sourceProfileId, const String& newDisplayName) const {
    ESP_LOGI(TAG, "copyProfile: Entry. sourceProfileId=%s, newDisplayName=%s", sourceProfileId.c_str(), newDisplayName.c_str());

    if (!_profileFileExists(sourceProfileId)) {
        ESP_LOGW(TAG, "copyProfile: Source profile '%s' does not exist.", sourceProfileId.c_str());
        return "";
    }

    String newProfileId = _generateNewProfileId();
    ESP_LOGI(TAG, "copyProfile: Generated newProfileId=%s", newProfileId.c_str());

    String sourceProfileContent;
    if (!getProfileContent(sourceProfileId, sourceProfileContent)) {
        ESP_LOGE(TAG, "copyProfile: Failed to get content of source profile %s.", sourceProfileId.c_str());
        return "";
    }
    ESP_LOGI(TAG, "copyProfile: Source profile content retrieved successfully.");

    JsonDocument profileDoc;
    const DeserializationError error = deserializeJson(profileDoc, sourceProfileContent);
    if (error) {
        ESP_LOGE(TAG, "copyProfile: Failed to deserialize source profile content for copying: %s", error.c_str());
        return "";
    }
    ESP_LOGI(TAG, "copyProfile: Source profile content deserialized successfully.");

    profileDoc["id"] = newProfileId;
    profileDoc["name"] = newDisplayName;
    profileDoc["createdAt"] = getCurrentTimestamp();
    ESP_LOGI(TAG, "copyProfile: Updated id, name and createdAt in profileDoc.");

    const String destPath = String(_profilesDir) + "/" + newProfileId + ".json";
    File destFile = _fs->open(destPath, "w");
    if (!destFile) {
        ESP_LOGE(TAG, "copyProfile: Failed to create destination profile for copying: %s", destPath.c_str());
        return "";
    }
    ESP_LOGI(TAG, "copyProfile: Destination file opened for writing: %s", destPath.c_str());

    if (serializeJson(profileDoc, destFile) == 0) {
        ESP_LOGE(TAG, "copyProfile: Failed to write copied profile data to file: %s", destPath.c_str());
        destFile.close();
        _fs->remove(destPath);
        return "";
    }
    destFile.close();
    ESP_LOGI(TAG, "copyProfile: Profile data written to destination file successfully.");

    if (_addProfileToMetadata(newProfileId, newDisplayName)) {
        ESP_LOGI(TAG, "copyProfile: Metadata updated successfully. Returning newProfileId=%s", newProfileId.c_str());
        return newProfileId;
    }

    ESP_LOGE(TAG, "copyProfile: Failed to update metadata. Returning empty string.");
    return "";
}

bool ProfileService::updateProfile(const String& profileId, const String& newDisplayName) const {
    JsonDocument metadataDoc; // Use JsonDocument
    if (!_readMetadata(metadataDoc)) {
        return false;
    }

    auto profiles = metadataDoc.as<JsonArray>();
    bool found = false;
    for (JsonObject profile : profiles) {
        if (profile["id"].as<String>() == profileId) {
            profile["name"] = newDisplayName;
            found = true;
            break;
        }
    }

    if (!found) {
        ESP_LOGW(TAG, "Profile ID '%s' not found in metadata for update.", profileId.c_str());
        return false;
    }

    if (!_writeMetadata(metadataDoc)) {
        return false;
    }

    const String filePath = String(_profilesDir) + "/" + profileId + ".json";
    if (_fs->exists(filePath)) {
        File profileFileRead = _fs->open(filePath, "r");
        if (!profileFileRead) {
            ESP_LOGE(TAG, "Failed to open profile data file for reading during name update: %s", filePath.c_str());
            return false;
        }

        JsonDocument profileDoc; // Use JsonDocument
        DeserializationError error = deserializeJson(profileDoc, profileFileRead);
        profileFileRead.close();

        if (error == DeserializationError::Ok) {
            profileDoc["name"] = newDisplayName;

            File profileFileWrite = _fs->open(filePath, "w");
            if (!profileFileWrite) {
                ESP_LOGE(TAG, "Failed to open profile data file for writing during name update: %s", filePath.c_str());
                return false;
            }
            if (serializeJson(profileDoc, profileFileWrite) == 0) {
                ESP_LOGE(TAG, "Failed to update name in profile data file: %s", filePath.c_str());
            }
            profileFileWrite.close();
        } else {
            ESP_LOGE(TAG, "Failed to deserialize profile %s for name update: %s", profileId.c_str(), error.c_str());
        }
    }
    return true;
}

bool ProfileService::_setActiveProfileInternal(const String& profileId) const {
    JsonDocument metadataDoc;
    if (!_readMetadata(metadataDoc)) {
        return false;
    }

    auto profiles = metadataDoc.as<JsonArray>();
    bool profileFound = false;
    for (JsonObject profile : profiles) {
        if (profile["id"].as<String>() == profileId) {
            profile["isApplied"] = true;
            profileFound = true;
        } else {
            profile["isApplied"] = false;
        }
    }

    if (!profileFound) {
        ESP_LOGW(TAG, "Profile ID '%s' not found in metadata to set as active.", profileId.c_str());
        return false;
    }

    return _writeMetadata(metadataDoc);
}

bool ProfileService::setActiveAndApplyProfile(const String& profileId) const {
    ESP_LOGI(TAG, "setActiveAndApplyProfile: Attempting to set profile %s as active and apply it.", profileId.c_str());

    // 1. Set the profile as active in metadata
    if (!_setActiveProfileInternal(profileId)) {
        ESP_LOGE(TAG, "setActiveAndApplyProfile: Failed to set profile %s as active.", profileId.c_str());
        return false;
    }
    ESP_LOGI(TAG, "setActiveAndApplyProfile: Profile %s successfully marked as active.", profileId.c_str());

    // 2. Apply the profile settings
    if (!_applyProfileInternal(profileId)) {
        ESP_LOGE(TAG, "setActiveAndApplyProfile: Failed to apply profile %s.", profileId.c_str());
        // Optionally, you might want to revert the 'isApplied' status here if apply fails
        // However, for simplicity, we'll just log and return false.
        return false;
    }
    ESP_LOGI(TAG, "setActiveAndApplyProfile: Profile %s successfully applied.", profileId.c_str());

    return true;
}

bool ProfileService::getProfileContent(const String& profileId, String& dest) const {
    const String filePath = String(_profilesDir) + "/" + profileId + ".json";
    File profileFile = _fs->open(filePath, "r");
    if (!profileFile) {
        ESP_LOGW(TAG, "Profile file not found for content retrieval: %s", filePath.c_str());
        return false;
    }

    JsonDocument doc;
    const DeserializationError error = deserializeJson(doc, profileFile);
    profileFile.close();
    if (error) {
        ESP_LOGE(TAG, "Failed to deserialize profile %s for content retrieval: %s", profileId.c_str(), error.c_str());
        return false;
    }

    serializeJson(doc, dest);
    return true;
}

void ProfileService::getProfileListAsJson(String& dest) const {
    const auto profileList = getProfileList();

    JsonDocument doc; // Use JsonDocument
    const auto array = doc.to<JsonArray>();

    for (const auto& meta : profileList) {
        auto profileObj = array.add<JsonObject>();
        profileObj["id"] = meta.id;
        profileObj["name"] = meta.name;
        profileObj["createdAt"] = meta.createdAt;
        profileObj["isApplied"] = meta.isApplied;
    }

    serializeJson(doc, dest);
}

bool ProfileService::saveCurrentSettingsToProfile(const String& profileId) const {
    // 1. Убедимся, что профиль существует
    if (!_profileFileExists(profileId)) {
        ESP_LOGE(TAG, "Cannot save settings. Profile ID '%s' not found.", profileId.c_str());
        return false;
    }

    // 2. Получим текущее имя профиля из метаданных
    JsonDocument metadataDoc; // Use JsonDocument
    if (!_readMetadata(metadataDoc)) {
        ESP_LOGE(TAG, "Failed to read metadata to get profile name for update.");
        return false;
    }

    String currentName;
    bool found = false;
    const JsonArray profiles = metadataDoc.as<JsonArray>();
    for (JsonObject profile : profiles) {
        if (profile["id"].as<String>() == profileId) {
            currentName = profile["name"].as<String>();
            found = true;
            break;
        }
    }

    if (!found) {
        // Неконсистентное состояние: файл есть, а в метаданных его нет.
        // Этого не должно происходить, но лучше обработать.
        ESP_LOGW(TAG, "Profile file '%s.json' exists but is not in metadata. Cannot update.", profileId.c_str());
        return false;
    }

    // 3. Вызовем существующий метод, который перезапишет файл профиля текущими настройками
    // Он также обновит 'createdAt', что логично для операции "сохранить".
    ESP_LOGI(TAG, "Saving current settings to profile ID '%s' (Name: %s)", profileId.c_str(), currentName.c_str());
    return _createOrUpdateProfile(profileId, currentName);
}
