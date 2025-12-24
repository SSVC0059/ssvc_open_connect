#ifndef SSVC_OPEN_CONNECT_FILEHANDLER_H
#define SSVC_OPEN_CONNECT_FILEHANDLER_H

#include "PsychicHttp.h"
#include "SecurityManager.h"
#include "FS.h"

class FileHandler {
public:
    FileHandler(FS* fs);
    void registerHandlers(PsychicHttpServer& server, SecurityManager* securityManager);

private:
    FS* _fs;
    void buildFileTree(JsonArray& parentArray, File& dir);
};

#endif //SSVC_OPEN_CONNECT_FILEHANDLER_H
