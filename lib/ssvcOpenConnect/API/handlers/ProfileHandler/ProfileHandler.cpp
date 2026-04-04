#include "ProfileHandler.h"
#include <ArduinoJson.h>
#include "esp_log.h" // Добавить для логирования

static auto TAG_PROFILE_HANDLER = "ProfileHandler"; // Добавить для логирования

ProfileHandler::ProfileHandler(ProfileService* profileService) : _profileService(profileService) {}

void ProfileHandler::handleGetProfiles(AsyncWebServerRequest* request) {
    String jsonResponse;
    ProfileService::getInstance()->getProfileListAsJson(jsonResponse);
    request->send(200, "application/json", jsonResponse.c_str());
}

void ProfileHandler::handleGetActiveProfile(AsyncWebServerRequest* request) {
    const String activeProfileId = ProfileService::getInstance()->getActiveProfileId();
    if (activeProfileId.isEmpty()) {
        request->send(404, "application/json", R"({"error": "No active profile found"})");
        return;
    }
    JsonDocument doc;
    doc["id"] = activeProfileId;
    String jsonResponse;
    serializeJson(doc, jsonResponse);
    request->send(200, "application/json", jsonResponse.c_str());
}

void ProfileHandler::handleGetProfileContent(AsyncWebServerRequest* request) {
    if (!request->hasParam("id")) {
        request->send(400, "application/json", R"({"error": "Missing 'id' parameter in request"})");
        return;
    }

    const String profileId = request->getParam("id")->value();

    if (profileId.isEmpty()) {
        request->send(400, "application/json", R"({"error": "Field 'id' cannot be empty"})");
        return;
    }

    String jsonResponse;
    if (ProfileService::getInstance()->getProfileContent(profileId, jsonResponse)) {
        request->send(200, "application/json", jsonResponse.c_str());
    } else {
        request->send(404, "application/json", R"({"error": "Profile content not found or failed to retrieve"})");
    }
}

void ProfileHandler::handleCreateProfile(AsyncWebServerRequest* request, JsonVariant& json) {
    if (!json.is<JsonObject>()) {
        request->send(400, "application/json", R"({"error": "Invalid JSON in request body"})");
        return;
    }

    if (!json["name"].is<const char*>()) {
        request->send(400, "application/json", R"({"error": "Missing or invalid 'name' field"})");
        return;
    }

    const String name = json["name"].as<String>();

    if (name.isEmpty()) {
        request->send(400, "application/json", R"({"error": "Field 'name' cannot be empty"})");
        return;
    }

    JsonObject content;
    if (!json["content"].isNull()) {
        if (json["content"].is<JsonObject>()) {
            content = json["content"].as<JsonObject>();
        } else {
            request->send(400, "application/json", R"({"error": "Invalid 'content' field, must be a JSON object"})");
            return;
        }
    }

    String newProfileId = ProfileService::getInstance()->createProfile(name, content);
    if (!newProfileId.isEmpty()) {
        JsonDocument responseDoc;
        responseDoc["success"] = true;
        responseDoc["id"] = newProfileId.c_str();
        String jsonResponse;
        serializeJson(responseDoc, jsonResponse);
        request->send(201, "application/json", jsonResponse.c_str());
    } else {
        request->send(500, "application/json", R"({"error": "Failed to save profile"})");
    }
}

void ProfileHandler::handleDeleteProfile(AsyncWebServerRequest* request, JsonVariant& json) {
    if (!json.is<JsonObject>()) {
        request->send(400, "application/json", R"({"error": "Invalid JSON in request body"})");
        return;
    }

    if (!json["id"].is<const char*>()) {
        request->send(400, "application/json", R"({"error": "Missing or invalid 'id' field in request body"})");
        return;
    }

    const String profileId = json["id"].as<String>();

    if (profileId.isEmpty()) {
        request->send(400, "application/json", R"({"error": "Field 'id' cannot be empty"})");
        return;
    }

    if (ProfileService::getInstance()->deleteProfile(profileId)) {
        request->send(200, "application/json", R"({"success": "Profile deleted"})");
    } else {
        request->send(404, "application/json", R"({"error": "Profile not found or failed to delete"})");
    }
}

void ProfileHandler::handleCopyProfile(AsyncWebServerRequest* request, JsonVariant& json) {
    ESP_LOGI(TAG_PROFILE_HANDLER, "handleCopyProfile: Entry");
    if (!json.is<JsonObject>()) {
        request->send(400, "application/json", R"({"error": "Invalid JSON in request body"})");
        return;
    }

    if (!json["sourceId"].is<const char*>()) {
        ESP_LOGE(TAG_PROFILE_HANDLER, "handleCopyProfile: Missing or invalid 'sourceId' field");
        request->send(400, "application/json", R"({"error": "Missing or invalid 'sourceId' field in request body"})");
        return;
    }
    if (!json["newName"].is<const char*>()) {
        ESP_LOGE(TAG_PROFILE_HANDLER, "handleCopyProfile: Missing or invalid 'newName' field");
        request->send(400, "application/json", R"({"error": "Missing or invalid 'newName' field in request body"})");
        return;
    }

    const String sourceProfileId = json["sourceId"].as<String>();
    const String newDisplayName = json["newName"].as<String>();

    ESP_LOGI(TAG_PROFILE_HANDLER, "handleCopyProfile: sourceId=%s, newName=%s", sourceProfileId.c_str(), newDisplayName.c_str());

    if (sourceProfileId.isEmpty() || newDisplayName.isEmpty()) {
        ESP_LOGE(TAG_PROFILE_HANDLER, "handleCopyProfile: 'sourceId' or 'newName' cannot be empty");
        request->send(400, "application/json", R"({"error": "Fields 'sourceId' and 'newName' cannot be empty"})");
        return;
    }

    String newProfileId = ProfileService::getInstance()->copyProfile(sourceProfileId, newDisplayName);
    ESP_LOGI(TAG_PROFILE_HANDLER, "handleCopyProfile: copyProfile returned newProfileId=%s", newProfileId.c_str());

    if (!newProfileId.isEmpty()) {
        JsonDocument responseDoc;
        responseDoc["success"] = true;
        responseDoc["id"] = newProfileId.c_str();
        String jsonResponse;
        serializeJson(responseDoc, jsonResponse);
        ESP_LOGI(TAG_PROFILE_HANDLER, "handleCopyProfile: Success, newProfileId=%s", newProfileId.c_str());
        request->send(201, "application/json", jsonResponse.c_str());
    } else {
        ESP_LOGE(TAG_PROFILE_HANDLER, "handleCopyProfile: Failed to copy profile. Source not found or internal error.");
        request->send(409, "application/json", R"({"error": "Failed to copy profile. source not found "})");
    }
}

void ProfileHandler::handleUpdateProfileMeta(AsyncWebServerRequest* request, JsonVariant& json) {
    if (!json.is<JsonObject>()) {
        request->send(400, "application/json", R"({"error": "Invalid JSON in request body"})");
        return;
    }

    if (!json["id"].is<const char*>()) {
        request->send(400, "application/json", R"({"error": "Missing or invalid 'id' field in request body"})");
        return;
    }
    if (!json["name"].is<const char*>()) {
        request->send(400, "application/json", R"({"error": "Missing or invalid 'name' field in request body"})");
        return;
    }

    const String profileId = json["id"].as<String>();
    const String newDisplayName = json["name"].as<String>();

    if (profileId.isEmpty() || newDisplayName.isEmpty()) {
        request->send(400, "application/json", R"({"error": "Fields 'id' and 'name' cannot be empty"})");
        return;
    }

    if (ProfileService::getInstance()->updateProfile(profileId, newDisplayName)) {
        request->send(200, "application/json", R"({"success": "Profile updated"})");
    } else {
        request->send(404, "application/json", R"({"error": "Profile not found or failed to update"})");
    }
}

void ProfileHandler::handleSetActiveAndApplyProfile(AsyncWebServerRequest* request, JsonVariant& json) {
    if (!json.is<JsonObject>()) {
        request->send(400, "application/json", R"({"error": "Invalid JSON in request body"})");
        return;
    }

    if (!json["id"].is<const char*>()) {
        ESP_LOGE(TAG_PROFILE_HANDLER, "handleSetActiveAndApplyProfile: Missing or invalid 'id' field");
        request->send(400, "application/json", R"({"error": "Missing or invalid 'id' field in request body"})");
        return;
    }

    const String profileId = json["id"].as<String>();

    if (profileId.isEmpty()) {
        ESP_LOGE(TAG_PROFILE_HANDLER, "handleSetActiveAndApplyProfile: 'id' field cannot be empty");
        request->send(400, "application/json", R"({"error": "Field 'id' cannot be empty"})");
        return;
    }

    if (ProfileService::getInstance()->setActiveAndApplyProfile(profileId)) {
        ESP_LOGI(TAG_PROFILE_HANDLER, "handleSetActiveAndApplyProfile: Profile %s set as active and applied successfully", profileId.c_str());
        request->send(200, "application/json", R"({"success": "Profile set as active and applied"})");
    } else {
        ESP_LOGE(TAG_PROFILE_HANDLER, "handleSetActiveAndApplyProfile: Failed to set profile %s as active and apply it.", profileId.c_str());
        request->send(500, "application/json", R"({"error": "Failed to set active and apply profile"})");
    }
}

void ProfileHandler::handleSaveSettingsToProfile(AsyncWebServerRequest* request, JsonVariant& json) {
    if (!json.is<JsonObject>()) {
        request->send(400, "application/json", R"({"error": "Invalid JSON in request body"})");
        return;
    }

    if (!json["id"].is<const char*>()) {
        request->send(400, "application/json", R"({"error": "Missing or invalid 'id' field in request body"})");
        return;
    }

    const String profileId = json["id"].as<String>();

    if (profileId.isEmpty()) {
        request->send(400, "application/json", R"({"error": "Field 'id' cannot be empty"})");
        return;
    }

    ESP_LOGI(TAG_PROFILE_HANDLER, "handleSaveSettingsToProfile called for profileId: %s", profileId.c_str());

    if (ProfileService::getInstance()->saveCurrentSettingsToProfile(profileId)) {
        request->send(200, "application/json", R"({"success": true, "message": "Current settings saved to profile."})");
    } else {
        request->send(404, "application/json", R"({"success": false, "message": "Profile not found or failed to save current settings."})");
    }
}

void ProfileHandler::handleUpdateProfileContent(AsyncWebServerRequest* request, JsonVariant& json) {
    if (!json.is<JsonObject>()) {
        request->send(400, "application/json", R"({"error": "Invalid JSON in request body"})");
        return;
    }

    if (!json["id"].is<const char*>()) {
        request->send(400, "application/json", R"({"error": "Missing or invalid 'id' field"})");
        return;
    }

    if (!json["content"].is<JsonObject>()) {
        request->send(400, "application/json", R"({"error": "Missing or invalid 'content' field"})");
        return;
    }

    const String profileId = json["id"].as<String>();
    const JsonObject content = json["content"].as<JsonObject>();

    if (profileId.isEmpty()) {
        request->send(400, "application/json", R"({"error": "Field 'id' cannot be empty"})");
        return;
    }

    if (ProfileService::getInstance()->updateProfileContent(profileId, content)) {
        request->send(200, "application/json", R"({"success": "Profile content updated"})");
    } else {
        request->send(404, "application/json", R"({"error": "Profile not found or failed to update content"})");
    }
}
