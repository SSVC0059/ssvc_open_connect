#include "FileHandler.h"
#include <ArduinoJson.h>

FileHandler::FileHandler(FS* fs) : _fs(fs) {}

void FileHandler::registerHandlers(AsyncWebServer& server, SecurityManager* securityManager) {
    auto listFilesHandler = [this](AsyncWebServerRequest* request) {
        JsonDocument doc;
        auto rootArray = doc.to<JsonArray>();

        File root = _fs->open("/");
        if (!root) {
            request->send(500, "text/plain", "Could not open root directory");
            return;
        }

        buildFileTree(rootArray, root);
        root.close();

        String jsonResponse;
        serializeJson(doc, jsonResponse);
        request->send(200, "application/json", jsonResponse.c_str());
    };
    server.on("/rest/files", HTTP_GET, securityManager->wrapRequest(listFilesHandler, AuthenticationPredicates::IS_AUTHENTICATED));

    auto downloadFileHandler = [this](AsyncWebServerRequest* request) {
        const String path = request->url();
        String prefix = "/rest/files";
        if (path.startsWith(prefix)) {
            String filePath = path.substring(prefix.length());
            if (filePath.isEmpty() || filePath == "/") {
                request->send(400, "text/plain", "File path cannot be empty");
                return;
            }

            File file = _fs->open(filePath, "r");
            if (!file || file.isDirectory()) {
                if (file) file.close();
                request->send(404, "text/plain", "File not found or is a directory");
                return;
            }
            file.close();
            request->send(*_fs, filePath, "application/octet-stream");
            return;
        }
        request->send(404);
    };
    server.on("/rest/files/*", HTTP_GET, securityManager->wrapRequest(downloadFileHandler, AuthenticationPredicates::IS_AUTHENTICATED));

    auto deleteFileHandler = [this](AsyncWebServerRequest* request) {
        const String path = request->url();
        const String prefix = "/rest/files";
        if (path.startsWith(prefix)) {
            const String filePath = path.substring(prefix.length());
            if (filePath.isEmpty()) {
                request->send(400, "text/plain", "File path cannot be empty");
                return;
            }
            if (_fs->remove(filePath)) {
                request->send(204);
            } else {
                request->send(500, "text/plain", "Failed to delete file");
            }
            return;
        }
        request->send(404);
    };
    server.on("/rest/files/*", HTTP_DELETE, securityManager->wrapRequest(deleteFileHandler, AuthenticationPredicates::IS_AUTHENTICATED));

    auto copyFileHandler = [this](AsyncWebServerRequest* request, JsonVariant& json) {
        if (!json.is<JsonObject>()) {
            request->send(400, "text/plain", "Invalid JSON body");
            return;
        }
        const String sourcePath = json["source"].as<String>();
        const String destPath = json["destination"].as<String>();

        if (sourcePath.isEmpty() || destPath.isEmpty()) {
            request->send(400, "text/plain", "Missing 'source' or 'destination' fields");
            return;
        }

        File sourceFile = _fs->open(sourcePath, "r");
        if (!sourceFile) {
            request->send(404, "text/plain", "Source file not found");
            return;
        }

        File destFile = _fs->open(destPath, "w");
        if (!destFile) {
            sourceFile.close();
            request->send(500, "text/plain", "Could not create destination file");
            return;
        }

        uint8_t buf[512];
        while (sourceFile.available()) {
            const size_t len = sourceFile.read(buf, sizeof(buf));
            destFile.write(buf, len);
        }

        sourceFile.close();
        destFile.close();
        request->send(201);
    };
    server.on("/rest/files/copy", HTTP_POST, securityManager->wrapCallback(copyFileHandler, AuthenticationPredicates::IS_AUTHENTICATED));
}

void FileHandler::buildFileTree(JsonArray& parentArray, File& dir) {
    File file = dir.openNextFile();
    while(file) {
        auto fileNode = parentArray.add<JsonObject>();

        String fullName(file.name());
        fileNode["name"] = fullName.substring(fullName.lastIndexOf('/') + 1);

        if (file.isDirectory()) {
            fileNode["type"] = "directory";
            auto children = fileNode["children"].to<JsonArray>();
            buildFileTree(children, file);
        } else {
            fileNode["type"] = "file";
            fileNode["size"] = file.size();
        }
        file.close();
        file = dir.openNextFile();
    }
}
