#include "ProfileHandler.h"
#include <ArduinoJson.h>
#include "esp_log.h" // Добавить для логирования

static auto TAG_PROFILE_HANDLER = "ProfileHandler"; // Добавить для логирования

ProfileHandler::ProfileHandler(ProfileService* profileService) : _profileService(profileService) {}

esp_err_t ProfileHandler::handleGetProfiles(PsychicRequest *request) {
    String jsonResponse;
    ProfileService::getInstance()->getProfileListAsJson(jsonResponse);
    return request->reply(200, "application/json", jsonResponse.c_str());
}

esp_err_t ProfileHandler::handleGetActiveProfile(PsychicRequest *request) {
    const String activeProfileId = ProfileService::getInstance()->getActiveProfileId();
    if (activeProfileId.isEmpty()) {
        return request->reply(404, "application/json", R"({"error": "No active profile found"})");
    }
    JsonDocument doc;
    doc["id"] = activeProfileId;
    String jsonResponse;
    serializeJson(doc, jsonResponse);
    return request->reply(200, "application/json", jsonResponse.c_str());
}

esp_err_t ProfileHandler::handleGetProfileContent(PsychicRequest *request) {
    if (!request->hasParam("id")) {
        return request->reply(400, "application/json", R"({"error": "Missing 'id' parameter in request"})");
    }

    const String profileId = request->getParam("id")->value();

    if (profileId.isEmpty()) {
        return request->reply(400, "application/json", R"({"error": "Field 'id' cannot be empty"})");
    }

    String jsonResponse;
    if (ProfileService::getInstance()->getProfileContent(profileId, jsonResponse)) { // Вызов getProfileContent
        return request->reply(200, "application/json", jsonResponse.c_str());
    } else {
        return request->reply(404, "application/json", R"({"error": "Profile content not found or failed to retrieve"})");
    }
}

esp_err_t ProfileHandler::handleCreateProfile(PsychicRequest *request) {
    JsonDocument doc;
    const DeserializationError error = deserializeJson(doc, request->body());

    if (error) {
        return request->reply(400, "application/json", R"({"error": "Invalid JSON in request body"})");
    }

    if (!doc["name"].is<const char*>()) {
        return request->reply(400, "application/json", R"({"error": "Missing or invalid 'name' field"})");
    }

    const String name = doc["name"].as<String>();

    if (name.isEmpty()) {
        return request->reply(400, "application/json", R"({"error": "Field 'name' cannot be empty"})");
    }

    String newProfileId = ProfileService::getInstance()->createProfile(name);
    if (!newProfileId.isEmpty()) {
        JsonDocument responseDoc;
        responseDoc["success"] = true;
        responseDoc["id"] = newProfileId;
        String jsonResponse;
        serializeJson(responseDoc, jsonResponse);
        return request->reply(201, "application/json", jsonResponse.c_str());
    } else {
        return request->reply(500, "application/json", R"({"error": "Failed to save profile"})");
    }
}

esp_err_t ProfileHandler::handleDeleteProfile(PsychicRequest *request) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, request->body());

    if (error) {
        return request->reply(400, "application/json", R"({"error": "Invalid JSON in request body"})");
    }

    if (!doc["id"].is<const char*>()) {
        return request->reply(400, "application/json", R"({"error": "Missing or invalid 'id' field in request body"})");
    }

    const String profileId = doc["id"].as<String>();

    if (profileId.isEmpty()) {
        return request->reply(400, "application/json", R"({"error": "Field 'id' cannot be empty"})");
    }

    if (ProfileService::getInstance()->deleteProfile(profileId)) {
        return request->reply(200, "application/json", R"({"success": "Profile deleted"})");
    } else {
        return request->reply(404, "application/json", R"({"error": "Profile not found or failed to delete"})");
    }
}

esp_err_t ProfileHandler::handleCopyProfile(PsychicRequest *request) {
    ESP_LOGI(TAG_PROFILE_HANDLER, "handleCopyProfile: Entry"); // Логирование входа
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, request->body());

    if (error) {
        ESP_LOGE(TAG_PROFILE_HANDLER, "handleCopyProfile: Invalid JSON in request body: %s", error.c_str()); // Логирование ошибки
        return request->reply(400, "application/json", R"({"error": "Invalid JSON in request body"})");
    }

    if (!doc["sourceId"].is<const char*>()) {
        ESP_LOGE(TAG_PROFILE_HANDLER, "handleCopyProfile: Missing or invalid 'sourceId' field"); // Логирование ошибки
        return request->reply(400, "application/json", R"({"error": "Missing or invalid 'sourceId' field in request body"})");
    }
    if (!doc["newName"].is<const char*>()) {
        ESP_LOGE(TAG_PROFILE_HANDLER, "handleCopyProfile: Missing or invalid 'newName' field"); // Логирование ошибки
        return request->reply(400, "application/json", R"({"error": "Missing or invalid 'newName' field in request body"})");
    }

    const String sourceProfileId = doc["sourceId"].as<String>();
    const String newDisplayName = doc["newName"].as<String>();

    ESP_LOGI(TAG_PROFILE_HANDLER, "handleCopyProfile: sourceId=%s, newName=%s", sourceProfileId.c_str(), newDisplayName.c_str()); // Логирование параметров

    if (sourceProfileId.isEmpty() || newDisplayName.isEmpty()) {
        ESP_LOGE(TAG_PROFILE_HANDLER, "handleCopyProfile: 'sourceId' or 'newName' cannot be empty"); // Логирование ошибки
        return request->reply(400, "application/json", R"({"error": "Fields 'sourceId' and 'newName' cannot be empty"})");
    }

    String newProfileId = ProfileService::getInstance()->copyProfile(sourceProfileId, newDisplayName);
    ESP_LOGI(TAG_PROFILE_HANDLER, "handleCopyProfile: copyProfile returned newProfileId=%s", newProfileId.c_str()); // Логирование результата

    if (!newProfileId.isEmpty()) {
        JsonDocument responseDoc;
        responseDoc["success"] = true;
        responseDoc["id"] = newProfileId;
        String jsonResponse;
        serializeJson(responseDoc, jsonResponse);
        ESP_LOGI(TAG_PROFILE_HANDLER, "handleCopyProfile: Success, newProfileId=%s", newProfileId.c_str()); // Логирование успеха
        return request->reply(201, "application/json", jsonResponse.c_str());
    }
    ESP_LOGE(TAG_PROFILE_HANDLER, "handleCopyProfile: Failed to copy profile. Source not found or internal error."); // Логирование ошибки
    return request->reply(409, "application/json", R"({"error": "Failed to copy profile. source not found "})");
}

esp_err_t ProfileHandler::handleUpdateProfileMeta(PsychicRequest *request) {
    JsonDocument doc;
    const DeserializationError error = deserializeJson(doc, request->body());

    if (error) {
        return request->reply(400, "application/json", R"({"error": "Invalid JSON in request body"})");
    }

    if (!doc["id"].is<const char*>()) {
        return request->reply(400, "application/json", R"({"error": "Missing or invalid 'id' field in request body"})");
    }
    if (!doc["name"].is<const char*>()) {
        return request->reply(400, "application/json", R"({"error": "Missing or invalid 'name' field in request body"})");
    }

    const String profileId = doc["id"].as<String>();
    const String newDisplayName = doc["name"].as<String>();

    if (profileId.isEmpty() || newDisplayName.isEmpty()) {
        return request->reply(400, "application/json", R"({"error": "Fields 'id' and 'name' cannot be empty"})");
    }

    if (ProfileService::getInstance()->updateProfile(profileId, newDisplayName)) {
        return request->reply(200, "application/json", R"({"success": "Profile updated"})");
    } else {
        return request->reply(404, "application/json", R"({"error": "Profile not found or failed to update"})");
    }
}

esp_err_t ProfileHandler::handleSetActiveAndApplyProfile(PsychicRequest *request) {
    JsonDocument doc;
    const DeserializationError error = deserializeJson(doc, request->body());

    if (error) {
        ESP_LOGE(TAG_PROFILE_HANDLER, "handleSetActiveAndApplyProfile: Invalid JSON in request body: %s", error.c_str());
        return request->reply(400, "application/json", R"({"error": "Invalid JSON in request body"})");
    }

    if (!doc["id"].is<const char*>()) {
        ESP_LOGE(TAG_PROFILE_HANDLER, "handleSetActiveAndApplyProfile: Missing or invalid 'id' field");
        return request->reply(400, "application/json", R"({"error": "Missing or invalid 'id' field in request body"})");
    }

    const String profileId = doc["id"].as<String>();

    if (profileId.isEmpty()) {
        ESP_LOGE(TAG_PROFILE_HANDLER, "handleSetActiveAndApplyProfile: 'id' field cannot be empty");
        return request->reply(400, "application/json", R"({"error": "Field 'id' cannot be empty"})");
    }

    if (ProfileService::getInstance()->setActiveAndApplyProfile(profileId)) {
        ESP_LOGI(TAG_PROFILE_HANDLER, "handleSetActiveAndApplyProfile: Profile %s set as active and applied successfully", profileId.c_str());
        return request->reply(200, "application/json", R"({"success": "Profile set as active and applied"})");
    } else {
        ESP_LOGE(TAG_PROFILE_HANDLER, "handleSetActiveAndApplyProfile: Failed to set profile %s as active and apply it.", profileId.c_str());
        return request->reply(500, "application/json", R"({"error": "Failed to set active and apply profile"})");
    }
}

esp_err_t ProfileHandler::handleSaveSettingsToProfile(PsychicRequest *request) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, request->body());

    if (error) {
        return request->reply(400, "application/json", R"({"error": "Invalid JSON in request body"})");
    }

    if (!doc["id"].is<const char*>()) {
        return request->reply(400, "application/json", R"({"error": "Missing or invalid 'id' field in request body"})");
    }

    const String profileId = doc["id"].as<String>();

    if (profileId.isEmpty()) {
        return request->reply(400, "application/json", R"({"error": "Field 'id' cannot be empty"})");
    }

    ESP_LOGI(TAG_PROFILE_HANDLER, "handleSaveSettingsToProfile called for profileId: %s", profileId.c_str());

    if (ProfileService::getInstance()->saveCurrentSettingsToProfile(profileId)) {
        return request->reply(200, "application/json", R"({"success": true, "message": "Current settings saved to profile."})");
    } else {
        return request->reply(404, "application/json", R"({"success": false, "message": "Profile not found or failed to save current settings."})");
    }
}

esp_err_t ProfileHandler::handleUpdateProfileContent(PsychicRequest *request) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, request->body());

    if (error) {
        return request->reply(400, "application/json", R"({"error": "Invalid JSON in request body"})");
    }

    if (!doc["id"].is<const char*>()) {
        return request->reply(400, "application/json", R"({"error": "Missing or invalid 'id' field"})");
    }

    if (!doc["content"].is<JsonObject>()) {
        return request->reply(400, "application/json", R"({"error": "Missing or invalid 'content' field"})");
    }

    const String profileId = doc["id"].as<String>();
    const JsonObject content = doc["content"].as<JsonObject>();

    if (profileId.isEmpty()) {
        return request->reply(400, "application/json", R"({"error": "Field 'id' cannot be empty"})");
    }

    if (ProfileService::getInstance()->updateProfileContent(profileId, content)) {
        return request->reply(200, "application/json", R"({"success": "Profile content updated"})");
    } else {
        return request->reply(404, "application/json", R"({"error": "Profile not found or failed to update content"})");
    }
}
