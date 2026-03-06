#include "FileHandler.h"
#include <ArduinoJson.h>
#include "PsychicFileResponse.h"

FileHandler::FileHandler(FS* fs) : _fs(fs) {}

void FileHandler::registerHandlers(PsychicHttpServer& server, SecurityManager* securityManager) {
    auto listFilesHandler = [this](PsychicRequest* request) -> esp_err_t {
        JsonDocument doc;
        auto rootArray = doc.to<JsonArray>();

        File root = _fs->open("/");
        if (!root) {
            return request->reply(500, "text/plain", "Could not open root directory");
        }

        buildFileTree(rootArray, root);
        root.close();

        String jsonResponse;
        serializeJson(doc, jsonResponse);
        return request->reply(200, "application/json", jsonResponse.c_str());
    };
    server.on("/rest/files", HTTP_GET, securityManager->wrapRequest(listFilesHandler, AuthenticationPredicates::IS_AUTHENTICATED));

    auto downloadFileHandler = [this](PsychicRequest* request) -> esp_err_t {
        const String path = request->path();
        String prefix = "/rest/files";
        if (path.startsWith(prefix)) {
            String filePath = path.substring(prefix.length());
            if (filePath.isEmpty() || filePath == "/") {
                return request->reply(400, "text/plain", "File path cannot be empty");
            }

            File file = _fs->open(filePath, "r");
            if (!file || file.isDirectory()) {
                return request->reply(404, "text/plain", "File not found or is a directory");
            }

            auto* response = new PsychicFileResponse(request, file, filePath, String(), true);
            return response->send();
        }
        return request->reply(404);
    };
    server.on("/rest/files/*", HTTP_GET, securityManager->wrapRequest(downloadFileHandler, AuthenticationPredicates::IS_AUTHENTICATED));

    auto deleteFileHandler = [this](PsychicRequest* request) -> esp_err_t {
        const String path = request->path();
        const String prefix = "/rest/files";
        if (path.startsWith(prefix)) {
            const String filePath = path.substring(prefix.length());
            if (filePath.isEmpty()) {
                return request->reply(400, "text/plain", "File path cannot be empty");
            }
            if (_fs->remove(filePath)) {
                return request->reply(204); // No Content
            } else {
                return request->reply(500, "text/plain", "Failed to delete file");
            }
        }
        return request->reply(404);
    };
    server.on("/rest/files/*", HTTP_DELETE, securityManager->wrapRequest(deleteFileHandler, AuthenticationPredicates::IS_AUTHENTICATED));

    auto copyFileHandler = [this](PsychicRequest* request) -> esp_err_t {
        JsonDocument doc;
        deserializeJson(doc, request->body());
        const String sourcePath = doc["source"];
        const String destPath = doc["destination"];

        if (sourcePath.isEmpty() || destPath.isEmpty()) {
            return request->reply(400, "text/plain", "Missing 'source' or 'destination' fields");
        }

        File sourceFile = _fs->open(sourcePath, "r");
        if (!sourceFile) {
            return request->reply(404, "text/plain", "Source file not found");
        }

        File destFile = _fs->open(destPath, "w");
        if (!destFile) {
            sourceFile.close();
            return request->reply(500, "text/plain", "Could not create destination file");
        }

        uint8_t buf[512];
        while (sourceFile.available()) {
            const size_t len = sourceFile.read(buf, sizeof(buf));
            destFile.write(buf, len);
        }

        sourceFile.close();
        destFile.close();
        return request->reply(201); // Created
    };
    server.on("/rest/files/copy", HTTP_POST, securityManager->wrapRequest(copyFileHandler, AuthenticationPredicates::IS_AUTHENTICATED));
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
