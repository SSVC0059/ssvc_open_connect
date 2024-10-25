#include "JsonSender.h"

// Конструктор
JsonSender::JsonSender() {
    randomSeed(analogRead(0));  // Инициализация генератора случайных чисел
}

// Метод для отправки JSON типа "common"
void JsonSender::sendCommonJson() {
    StaticJsonDocument<200> jsonDoc;
    jsonDoc["type"] = "common";
    jsonDoc["mmhg"] = 747;
    jsonDoc["tp1"] = 33.56;
    jsonDoc["tp2"] = 35.75;
    jsonDoc["relay"] = 0;
    jsonDoc["signal"] = 0;

    serializeJson(jsonDoc, Serial);
    Serial.println();
}

// Метод для отправки JSON типа "tp1_waiting"
void JsonSender::sendTp1WaitingJson() {
    JsonDocument jsonDoc;
    jsonDoc["type"] = "tp1_waiting";
    jsonDoc["tp1_target"] = tp1Value;

    JsonObject common = jsonDoc.createNestedObject("common");
    common["mmhg"] = 750;
    common["tp1"] = tp1Value;
    common["tp2"] = 30.81;
    common["relay"] = 1;
    common["signal"] = 0;

    serializeJson(jsonDoc, Serial);
    Serial.println();

    // Обновляем значение tp1
    tp1Value += 5;
    if (tp1Value > 70) {
        tp1Value = 25;
    }
}

// Метод для отправки ответа на запрос "GET_SETTINGS"
void JsonSender::sendSettingsResponse() {
    JsonDocument jsonDoc;
    jsonDoc["type"] = "response";
    jsonDoc["request"] = "GET_SETTINGS";
    jsonDoc["result"] = "OK";

    JsonObject settings = jsonDoc.createNestedObject("settings");

    JsonArray heads = settings.createNestedArray("heads");
    heads.add(30.1);
    heads.add(359);

    JsonArray hearts = settings.createNestedArray("hearts");
    hearts.add(2.1);
    hearts.add(4);

    settings["hyst"] = 2.5;
    settings["decrement"] = 10;

    JsonArray tails = settings.createNestedArray("tails");
    tails.add(2.1);
    tails.add(4);

    settings["sound"] = 0;
    settings["pressure"] = 1;
    settings["relay_inverted"] = 0;
    settings["relay_autostart"] = 0;
    settings["autoresume"] = 0;
    settings["auto_mode"] = 1;
    settings["head_timer"] = 900;
    settings["body_timer"] = 420;
    settings["tail_temp"] = 95.9;
    settings["start_delay"] = 600;
    settings["hearts_finish_temp"] = 90.3;

    JsonArray parallel_v3 = settings.createNestedArray("parallel_v3");
    JsonArray v3_item1 = parallel_v3.createNestedArray();
    v3_item1.add(0);
    v3_item1.add(0.4);
    v3_item1.add(10);

    JsonArray v3_item2 = parallel_v3.createNestedArray();
    v3_item2.add(81.0);
    v3_item2.add(0.5);
    v3_item2.add(11);

    JsonArray v3_item3 = parallel_v3.createNestedArray();
    v3_item3.add(86.0);
    v3_item3.add(0.6);
    v3_item3.add(12);

    JsonArray v3_item4 = parallel_v3.createNestedArray();
    v3_item4.add(96.0);
    v3_item4.add(0.7);
    v3_item4.add(13);

    JsonArray parallel_v1 = settings.createNestedArray("parallel_v1");
    parallel_v1.add(0.3);
    parallel_v1.add(10);

    settings["hearts_temp_shift"] = 1;
    settings["hearts_pause"] = 1;
    settings["formula"] = 1;
    settings["formula_start_temp"] = 85.3;
    settings["tank_mmhg"] = 6;
    settings["tp2_shift"] = 0.9;
    settings["tp_filter"] = 1;
    settings["signal_tp1_control"] = 1;
    settings["signal_inverted"] = 0;
    settings["tp1_control_temp"] = 60;
    settings["tp1_control_start"] = 1;
    settings["stab_limit_time"] = 900;
    settings["stab_limit_finish"] = 1;
    settings["backlight"] = "off";

    serializeJson(jsonDoc, Serial);
    Serial.println();
}
