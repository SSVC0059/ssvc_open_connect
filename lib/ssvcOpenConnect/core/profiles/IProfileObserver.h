#ifndef IProfileObserver_h
#define IProfileObserver_h

#include <ArduinoJson.h>

class IProfileObserver {
public:
    virtual ~IProfileObserver() = default;
    virtual const char* getProfileKey() const = 0;
    virtual void onProfileSave(JsonObject& dest) = 0;
    virtual void onProfileApply(const JsonObject& src) = 0;
};

#endif // IProfileObserver_h
