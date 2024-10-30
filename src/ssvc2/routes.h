#ifndef ROUTES_H
#define ROUTES_H

#include <ESPAsyncWebServer.h>
#include "UartTasks.h"

void initRoutes(AsyncWebServer& server);
void handleGetSetingSSVC(AsyncWebServerRequest *request);

#endif // ROUTES_H
