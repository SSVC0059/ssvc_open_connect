/**
 * Native тесты парсинга RectificationProcess (RectificationParseUtils)
 * Без Arduino/ESP32 — чистая логика преобразования строк телеметрии.
 *
 * Документация: docs/develop/testing-candidates.md
 */
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>
#include <string>
#include "core/rectification/RectificationTypes.h"
#include "core/rectification/RectificationParseUtils.h"

using namespace RectificationTypes;
using namespace RectificationParseUtils;

TEST_CASE("RectificationParseUtils - StringToStage") {
    CHECK(stringToStage("waiting") == Stage::WAITING);
    CHECK(stringToStage("tp1_waiting") == Stage::TP1_WAITING);
    CHECK(stringToStage("delayed_start") == Stage::DELAYED_START);
    CHECK(stringToStage("heads") == Stage::HEADS);
    CHECK(stringToStage("late_heads") == Stage::LATE_HEADS);
    CHECK(stringToStage("hearts") == Stage::HEARTS);
    CHECK(stringToStage("tails") == Stage::TAILS);
    CHECK(stringToStage("settings") == Stage::SETTINGS);
    CHECK(stringToStage("unknown") == Stage::PARSE_ERROR);
    CHECK(stringToStage("") == Stage::PARSE_ERROR);
}

TEST_CASE("RectificationParseUtils - StageToString") {
    CHECK(stageToString(Stage::WAITING) == "waiting");
    CHECK(stageToString(Stage::HEADS) == "heads");
    CHECK(stageToString(Stage::HEARTS) == "hearts");
    CHECK(stageToString(Stage::TAILS) == "tails");
    CHECK(stageToString(Stage::EMPTY) == "empty");
    CHECK(stageToString(Stage::PARSE_ERROR) == "error");
    CHECK(stageToString(Stage::SETTINGS) == "settings");
}

TEST_CASE("RectificationParseUtils - StageRoundTrip") {
    const char* stages[] = {"waiting", "heads", "hearts", "tails", "late_heads", "settings"};
    for (const char* s : stages) {
        Stage stage = stringToStage(s);
        std::string back = stageToString(stage);
        CHECK(back == s);
    }
}

TEST_CASE("RectificationParseUtils - TranslateStage") {
    CHECK(translateStage("heads") == "Головы");
    CHECK(translateStage("hearts") == "Тело");
    CHECK(translateStage("tails") == "Хвосты");
    CHECK(translateStage("waiting") == "Дежурный режим");
    CHECK(translateStage("late_heads") == "Подголовники");
    CHECK(translateStage("unknown") == "unknown");
}

TEST_CASE("RectificationParseUtils - StringToEvent") {
    CHECK(stringToEvent("heads_finished") == Event::HEADS_FINISHED);
    CHECK(stringToEvent("hearts_finished") == Event::HEARTS_FINISHED);
    CHECK(stringToEvent("tails_finished") == Event::TAILS_FINISHED);
    CHECK(stringToEvent("ds_error") == Event::DS_ERROR);
    CHECK(stringToEvent("ds_error_stop") == Event::DS_ERROR_STOP);
    CHECK(stringToEvent("remote_stop") == Event::REMOTE_STOP);
    CHECK(stringToEvent("manually_closed") == Event::MANUALLY_CLOSED);
    CHECK(stringToEvent("manually_opened") == Event::MANUALLY_OPENED);
    CHECK(stringToEvent("unknown_event") == Event::PARSE_ERROR);
}

TEST_CASE("RectificationParseUtils - EventToString") {
    CHECK(eventToString(Event::HEADS_FINISHED) == "heads_finished");
    CHECK(eventToString(Event::HEARTS_FINISHED) == "hearts_finished");
    CHECK(eventToString(Event::TAILS_FINISHED) == "tails_finished");
    CHECK(eventToString(Event::DS_ERROR) == "ds_error");
    CHECK(eventToString(Event::REMOTE_STOP) == "remote_stop");
}

TEST_CASE("RectificationParseUtils - EventRoundTrip") {
    const char* events[] = {"heads_finished", "hearts_finished", "tails_finished",
                           "ds_error", "ds_error_stop", "remote_stop",
                           "manually_closed", "manually_opened", "stabilization_limit"};
    for (const char* s : events) {
        Event ev = stringToEvent(s);
        std::string back = eventToString(ev);
        CHECK(back == s);
    }
}

TEST_CASE("RectificationParseUtils - ProcessStateToString") {
    CHECK(processStateToString(ProcessState::IDLE) == "idle");
    CHECK(processStateToString(ProcessState::RUNNING) == "running");
    CHECK(processStateToString(ProcessState::PAUSED) == "paused");
    CHECK(processStateToString(ProcessState::FINISHED) == "finished");
    CHECK(processStateToString(ProcessState::SKIPPED) == "skipped");
}

TEST_CASE("RectificationParseUtils - EventToDescription") {
    CHECK(eventToDescription(Event::HEADS_FINISHED).find("голов") != std::string::npos);
    CHECK(eventToDescription(Event::DS_ERROR).find("датчик") != std::string::npos);
    CHECK(eventToDescription(Event::REMOTE_STOP).find("остановки") != std::string::npos);
}
