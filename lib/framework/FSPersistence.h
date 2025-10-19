#ifndef FSPersistence_h
#define FSPersistence_h

/**
 *   ESP32 SvelteKit
 *
 *   A simple, secure and extensible framework for IoT projects for ESP32 platforms
 *   with responsive Sveltekit front-end built with TailwindCSS and DaisyUI.
 *   https://github.com/theelims/ESP32-sveltekit
 *
 *   Copyright (C) 2018 - 2023 rjwats
 *   Copyright (C) 2023 - 2024 theelims
 *
 *   All Rights Reserved. This software may be modified and distributed under
 *   the terms of the LGPL v3 license. See the LICENSE file for details.
 **/

#include <StatefulService.h>
#include <FS.h>

template <class T>
class FSPersistence
{
public:
    FSPersistence(JsonStateReader<T> stateReader,
                  JsonStateUpdater<T> stateUpdater,
                  StatefulService<T> *statefulService,
                  FS *fs,
                  const char *filePath) : _stateReader(stateReader),
                                          _stateUpdater(stateUpdater),
                                          _statefulService(statefulService),
                                          _fs(fs),
                                          _filePath(filePath),
                                          _updateHandlerId(0)
    {
        // Лог 1: Информирование о старте и включении автоматического обработчика
        ESP_LOGI(TAG, "Initialized for file: %s. Update handler enabled by default.", _filePath);
        enableUpdateHandler();
    }

    void readFromFS()
    {
        ESP_LOGI(TAG, "Starting READ operation for %s.", _filePath); // Лог 2: Начало чтения
        File settingsFile = _fs->open(_filePath, "r");

        if (settingsFile)
        {
            ESP_LOGD(TAG, "File opened successfully. Size: %lu bytes. Attempting deserialization.", settingsFile.size());

            // Проверка на пустой файл (может быть причиной сбоя)
            if (settingsFile.size() == 0) {
                ESP_LOGW(TAG, "File %s is empty. Applying defaults.", _filePath);
                settingsFile.close();
                goto failed_load;
            }

            JsonDocument jsonDocument;
            const DeserializationError error = deserializeJson(jsonDocument, settingsFile);

            if (error == DeserializationError::Ok && jsonDocument.is<JsonObject>())
            {
                ESP_LOGI(TAG, "Deserialization SUCCESSFUL. Updating service state."); // Лог 3: Успешная загрузка
                auto jsonObject = jsonDocument.as<JsonObject>();
                _statefulService->updateWithoutPropagation(jsonObject, _stateUpdater);
                settingsFile.close();
                return;
            }

            // Лог 4: Ошибка десериализации
            ESP_LOGE(TAG, "Deserialization FAILED for %s. Error: %s. Data is corrupt.", _filePath, error.c_str());
            settingsFile.close();
        }
        else
        {
            // Лог 5: Ошибка открытия файла (самый критичный лог)
            ESP_LOGW(TAG, "File open FAILED for %s. Check FS mount or file existence.", _filePath);
        }

    failed_load:
        // Если мы достигли этого места, загрузка не удалась.
        // Здесь происходит деструктивная перезапись, если не была отключена
        // (на что вы указали, что не сработало, т.к. код попадает сюда).
        applyDefaults();
        ESP_LOGW(TAG, "Applying defaults and invoking writeToFS() to ensure persistence of new defaults."); // Лог 6: Запись дефолтов
        writeToFS();
    }

    bool writeToFS()
    {
        ESP_LOGI(TAG, "Starting WRITE operation for %s.", _filePath);
        // create and populate a new json object
        JsonDocument jsonDocument;
        JsonObject jsonObject = jsonDocument.to<JsonObject>();
        _statefulService->read(jsonObject, _stateReader);

        // make directories if required
        mkdirs();

        // serialize it to filesystem
        File settingsFile = _fs->open(_filePath, "w");

        // failed to open file, return false
        if (!settingsFile)
        {
            ESP_LOGE(TAG, "File open for WRITING FAILED for %s. Cannot save state.", _filePath);
            return false;
        }

        // serialize the data to the file
        serializeJson(jsonDocument, settingsFile);
        settingsFile.close();
        ESP_LOGI(TAG, "State successfully written to %s.", _filePath);
        return true;
    }


    void disableUpdateHandler()
    {
        if (_updateHandlerId)
        {
            ESP_LOGW(TAG, "Disabling automatic update handler.");
            _statefulService->removeUpdateHandler(_updateHandlerId);
            _updateHandlerId = 0;
        }
    }

    void enableUpdateHandler()
    {
        if (!_updateHandlerId)
        {
            ESP_LOGI(TAG, "Enabling automatic update handler.");
            _updateHandlerId = _statefulService->addUpdateHandler([&](const String &originId)
                                                                  { writeToFS(); });
        }
    }

private:
    JsonStateReader<T> _stateReader;
    JsonStateUpdater<T> _stateUpdater;
    StatefulService<T> *_statefulService;
    FS *_fs;
    const char *_filePath;
    update_handler_id_t _updateHandlerId;

    // We assume we have a _filePath with format "/directory1/directory2/filename"
    // We create a directory for each missing parent
    void mkdirs()
    {
        String path(_filePath);
        int index = 0;
        while ((index = path.indexOf('/', index + 1)) != -1)
        {
            String segment = path.substring(0, index);
            if (!_fs->exists(segment))
            {
                _fs->mkdir(segment);
            }
        }
    }

    static constexpr auto TAG = "FSPersistence";

protected:
    // We assume the updater supplies sensible defaults if an empty object
    // is supplied, this virtual function allows that to be changed.
    virtual void applyDefaults()
    {
        JsonDocument jsonDocument;
        JsonObject jsonObject = jsonDocument.as<JsonObject>();
        _statefulService->updateWithoutPropagation(jsonObject, _stateUpdater);
    }
};

#endif // end FSPersistence
