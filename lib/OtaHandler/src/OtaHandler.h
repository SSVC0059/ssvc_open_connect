#ifndef OTAHANDLER_H
#define OTAHANDLER_H

#include <ArduinoOTA.h>

class OtaHandler {
public:
    OtaHandler();
    void begin();
    void handle();
};

#endif
