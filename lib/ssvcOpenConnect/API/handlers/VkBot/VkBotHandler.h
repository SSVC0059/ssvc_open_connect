#ifndef VK_BOT_HANDLER_H
#define VK_BOT_HANDLER_H

#include <ESPAsyncWebServer.h>
#include <AsyncJson.h>

class VkBotHandler {
public:
    VkBotHandler() = default;

    static void updateSettings(AsyncWebServerRequest* request, JsonVariant& json);
    static void getSettings(AsyncWebServerRequest* request);

private:
    static constexpr const char* TAG = "VkBot";
};

#endif
