#ifndef TELEGRAMSETTINGSSERVICE_H
#define TELEGRAMSETTINGSSERVICE_H

#include <StatefulService.h>
#include <FSPersistence.h>
#include "core/GlobalConfig/GlobalConfig.h"
#include <ESP32SvelteKit.h>
#include "core/profiles/IProfileObserver.h"

class PsychicHttpServer;

struct TelegramSettings {
    String botToken;
    String chatId;
};

StateUpdateResult updateTelegramSettings(JsonObject& root, TelegramSettings& settings);
void readTelegramSettings(TelegramSettings& settings, JsonObject& root);

class TelegramSettingsService : public StatefulService<TelegramSettings>, public IProfileObserver {
public:
    static TelegramSettingsService* getInstance();
    static void setInstance(TelegramSettingsService* instance);

    void begin();

    TelegramSettingsService(PsychicHttpServer* server, ESP32SvelteKit* esp32sveltekit);

    // IProfileObserver implementation
    const char* getProfileKey() const override;
    void onProfileApply(const JsonObject& profile) override;
    void onProfileSave(JsonObject& profile) override;

private:
    void migrateFromNvs();
    FSPersistence<TelegramSettings> _fsPersistence;
    ESP32SvelteKit* _esp32sveltekit;

    static TelegramSettingsService* _instance;
};

#endif // TELEGRAMSETTINGSSERVICE_H
