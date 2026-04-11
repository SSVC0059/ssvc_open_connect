#ifndef SSVC_OPEN_CONNECT_FILEHANDLER_H
#define SSVC_OPEN_CONNECT_FILEHANDLER_H

#include <ESPAsyncWebServer.h>
#include "SecurityManager.h"
#include "FS.h"

class FileHandler {
public:
    explicit FileHandler(FS* fs);
    void registerHandlers(AsyncWebServer& server, SecurityManager* securityManager);

private:
    FS* _fs;
    void buildFileTree(JsonArray& parentArray, File& dir);
};

#endif //SSVC_OPEN_CONNECT_FILEHANDLER_H
