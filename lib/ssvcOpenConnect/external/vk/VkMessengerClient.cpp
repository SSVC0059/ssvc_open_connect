#include "VkMessengerClient.h"

#include <algorithm>
#include <cstring>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

#include <HTTPClient.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <cstdarg>
#include <cstdio>

#include "commons/commons.h"
#include "commons/JsonSpiRamAllocator.h"
#include "core/SsvcConnector.h"
#include "core/SsvcOpenConnect.h"
#include "core/profiles/ProfileService.h"
#include "core/StatefulServices/VkSettingsService/VkSettingsService.h"
#include "core/StatefulServices/SensorDataService/SensorDataService.h"
#include "core/rectification/RectificationProcess.h"
#include "components/Zone/SensorZone.h"

namespace {

constexpr size_t kAlertMsgMax = 144;
constexpr size_t kAlertQueueDepth = 4;
constexpr int kLivePeriodTicks = 10;  // 10 * 500ms = 5s

struct VkAlertMsg {
    char line[kAlertMsgMax];
};

/** DS18B20-style ROM id: only last 4 characters in live VK text (compact). */
const char* sensorIdSuffix(const std::string& id) {
    const size_t len = id.size();
    if (len <= 4) {
        return id.c_str();
    }
    return id.c_str() + (len - 4);
}

void appendUrlEncoded(const char* plain, char* dst, size_t* pos, size_t cap) {
    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(plain); *p && *pos + 4 < cap; ++p) {
        const unsigned char c = *p;
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~') {
            dst[(*pos)++] = static_cast<char>(c);
        } else if (c == ' ') {
            dst[(*pos)++] = '+';
        } else {
            static const char* hex = "0123456789ABCDEF";
            dst[(*pos)++] = '%';
            dst[(*pos)++] = hex[(c >> 4) & 0xF];
            dst[(*pos)++] = hex[c & 0xF];
        }
    }
    dst[*pos] = '\0';
}

void appendFormKV(const char* key, const char* value, char* dst, size_t* pos, size_t cap) {
    if (*pos > 0 && *pos + 1 < cap) {
        dst[(*pos)++] = '&';
    }
    const size_t kl = strlen(key);
    if (*pos + kl + 1 >= cap) {
        return;
    }
    memcpy(dst + *pos, key, kl);
    *pos += kl;
    if (*pos + 1 >= cap) {
        return;
    }
    dst[(*pos)++] = '=';
    appendUrlEncoded(value, dst, pos, cap);
}

int64_t parsePeerId(const String& s) {
    if (s.isEmpty()) {
        return 0;
    }
    return static_cast<int64_t>(strtoll(s.c_str(), nullptr, 10));
}

struct VkBoldSpan {
    size_t u16off;
    size_t u16len;
};

static bool utf8_decode_at(const char* s, size_t rem, uint32_t& cp, size_t& adv) {
    if (rem == 0) {
        return false;
    }
    const unsigned char c0 = static_cast<unsigned char>(s[0]);
    if (c0 < 0x80) {
        cp = c0;
        adv = 1;
        return true;
    }
    if (rem >= 2 && (c0 & 0xE0) == 0xC0) {
        cp = (static_cast<uint32_t>(c0 & 0x1F) << 6) | (static_cast<unsigned char>(s[1]) & 0x3F);
        adv = 2;
        return true;
    }
    if (rem >= 3 && (c0 & 0xF0) == 0xE0) {
        cp = (static_cast<uint32_t>(c0 & 0x0F) << 12) |
             ((static_cast<uint32_t>(static_cast<unsigned char>(s[1]) & 0x3F)) << 6) |
             (static_cast<unsigned char>(s[2]) & 0x3F);
        adv = 3;
        return true;
    }
    if (rem >= 4 && (c0 & 0xF8) == 0xF0) {
        cp = (static_cast<uint32_t>(c0 & 0x07) << 18) |
             ((static_cast<uint32_t>(static_cast<unsigned char>(s[1]) & 0x3F)) << 12) |
             ((static_cast<uint32_t>(static_cast<unsigned char>(s[2]) & 0x3F)) << 6) |
             (static_cast<unsigned char>(s[3]) & 0x3F);
        adv = 4;
        return true;
    }
    cp = 0xFFFD;
    adv = 1;
    return true;
}

static size_t utf8_prefix_utf16_len(const char* s, size_t maxBytes) {
    size_t u16 = 0;
    size_t pos = 0;
    while (pos < maxBytes && s[pos] != '\0') {
        uint32_t cp = 0;
        size_t adv = 0;
        utf8_decode_at(s + pos, maxBytes - pos, cp, adv);
        u16 += (cp > 0xFFFF) ? 2u : 1u;
        pos += adv;
    }
    return u16;
}

static void serializeVkFormatData(const VkBoldSpan* spans, int ns, char* fmtOut, size_t fmtCap) {
    if (fmtOut == nullptr || fmtCap == 0) {
        return;
    }
    fmtOut[0] = '\0';
    if (spans == nullptr || ns <= 0) {
        return;
    }
    JsonDocument doc;
    doc["version"] = "1";
    JsonArray items = doc["items"].to<JsonArray>();
    for (int i = 0; i < ns; ++i) {
        JsonObject it = items.add<JsonObject>();
        it["type"] = "bold";
        it["offset"] = spans[i].u16off;
        it["length"] = spans[i].u16len;
    }
    if (serializeJson(doc, fmtOut, fmtCap) == 0) {
        fmtOut[0] = '\0';
    }
}

static constexpr int kVkLiveMaxSpans = 16;

static bool append_utf8(const char* s, char* buf, size_t& n, size_t cap, size_t& u16) {
    const size_t L = strlen(s);
    if (n + L >= cap) {
        return false;
    }
    memcpy(buf + n, s, L);
    n += L;
    buf[n] = '\0';
    u16 += utf8_prefix_utf16_len(s, L);
    return true;
}

static bool append_bold_lit(const char* lit, VkBoldSpan* spans, int& ns, char* buf, size_t& n, size_t cap,
                            size_t& u16) {
    if (ns >= kVkLiveMaxSpans) {
        return false;
    }
    const size_t L = strlen(lit);
    const size_t ulen = utf8_prefix_utf16_len(lit, L);
    spans[ns].u16off = u16;
    spans[ns].u16len = ulen;
    ++ns;
    return append_utf8(lit, buf, n, cap, u16);
}

static bool append_snprintf_chunk(char* buf, size_t& n, size_t cap, size_t& u16, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    const int w = vsnprintf(buf + n, cap - n, fmt, ap);
    va_end(ap);
    if (w < 0 || static_cast<size_t>(w) >= cap - n) {
        return false;
    }
    u16 += utf8_prefix_utf16_len(buf + n, static_cast<size_t>(w));
    n += static_cast<size_t>(w);
    buf[n] = '\0';
    return true;
}

static JsonSpiRamAllocator g_profileJsonAlloc;

static bool isSecretJsonKey(const char* key) {
    if (key == nullptr || key[0] == '\0') {
        return false;
    }
    char low[96];
    size_t i = 0;
    for (; key[i] != '\0' && i < sizeof(low) - 1; ++i) {
        const unsigned char c = static_cast<unsigned char>(key[i]);
        low[i] = (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : static_cast<char>(c);
    }
    if (key[i] != '\0') {
        return false;
    }
    low[i] = '\0';
    if (strcmp(low, "access_token") == 0 || strcmp(low, "bottoken") == 0 || strcmp(low, "password") == 0 ||
        strcmp(low, "client_secret") == 0 || strcmp(low, "refresh_token") == 0 || strcmp(low, "api_key") == 0 ||
        strcmp(low, "private_key") == 0 || strcmp(low, "mqtt_password") == 0 || strcmp(low, "wifi_password") == 0 ||
        strcmp(low, "psk") == 0 || strcmp(low, "authorization") == 0) {
        return true;
    }
    const size_t len = strlen(low);
    if (len >= 6 && strcmp(low + len - 6, "_token") == 0) {
        return true;
    }
    if (len >= 9 && strcmp(low + len - 9, "_password") == 0) {
        return true;
    }
    if (strstr(low, "secret") != nullptr) {
        return true;
    }
    return false;
}

static void redactSecretsInJson(JsonVariant v) {
    if (v.is<JsonObject>()) {
        JsonObject o = v.as<JsonObject>();
        for (JsonPair kv : o) {
            if (isSecretJsonKey(kv.key().c_str())) {
                kv.value().set("***");
            } else {
                redactSecretsInJson(kv.value());
            }
        }
    } else if (v.is<JsonArray>()) {
        for (JsonVariant el : v.as<JsonArray>()) {
            redactSecretsInJson(el);
        }
    }
}

static void stripSurroundingJsonQuotes(char* s) {
    const size_t L = strlen(s);
    if (L >= 2 && s[0] == '"' && s[L - 1] == '"') {
        s[L - 1] = '\0';
        memmove(s, s + 1, L - 1);
    }
}

static bool openConnectRelayRulesHasRules(JsonVariant relayRoot) {
    if (!relayRoot.is<JsonObject>()) {
        return false;
    }
    const JsonObject o = relayRoot.as<JsonObject>();
    if (!o["rules"].is<JsonArray>()) {
        return false;
    }
    return o["rules"].as<JsonArray>().size() > 0;
}

static void appendJsonFieldLine(const char* key, JsonVariant v, char* buf, size_t& n, size_t cap, size_t& u16, bool& ok) {
    if (!ok || key == nullptr) {
        return;
    }
    char valBuf[400];
    valBuf[0] = '\0';
    const size_t w = serializeJson(v, valBuf, sizeof(valBuf));
    if (w == 0) {
        ok = append_snprintf_chunk(buf, n, cap, u16, "%s: \n", key);
        return;
    }
    valBuf[sizeof(valBuf) - 1] = '\0';
    if (!v.is<JsonObject>() && !v.is<JsonArray>()) {
        stripSurroundingJsonQuotes(valBuf);
    }
    ok = append_snprintf_chunk(buf, n, cap, u16, "%s: %s\n", key, valBuf);
}

static void appendJsonObjectFlatLines(JsonObject o, char* buf, size_t& n, size_t cap, size_t& u16, bool& ok) {
    for (JsonPair kv : o) {
        if (!ok) {
            return;
        }
        appendJsonFieldLine(kv.key().c_str(), kv.value(), buf, n, cap, u16, ok);
    }
}

/** «Отобрано, мл:» только если есть ненулевые объёмы; строки с нулём не выводятся. Порядок: головы, подголовники,
 *  тело, хвосты (последнее — только при поддержке хвостов). Перед блоком — «--------». */
static void appendCollectedMlSection(RectificationProcess& rp, char* buf, size_t& n, size_t cap, size_t& u16,
                                     VkBoldSpan* spans, int& ns, bool& ok) {
    if (!ok) {
        return;
    }
    const bool tailsUi = SsvcSettings::init().isSupportTails();
    const int mlHeads = rp.getFlowVolumeCollectedMl(RectificationTypes::Stage::HEADS);
    const int mlLate = rp.getFlowVolumeCollectedMl(RectificationTypes::Stage::LATE_HEADS);
    const int mlHearts = rp.getFlowVolumeCollectedMl(RectificationTypes::Stage::HEARTS);
    const int mlTails = tailsUi ? rp.getFlowVolumeCollectedMl(RectificationTypes::Stage::TAILS) : 0;

    const bool any =
        (mlHeads > 0) || (mlLate > 0) || (mlHearts > 0) || (tailsUi && mlTails > 0);
    if (!any) {
        return;
    }
    ok = ok && append_utf8("--------\n", buf, n, cap, u16);
    static const char kVol[] = "Отобрано, мл:";
    ok = ok && append_bold_lit(kVol, spans, ns, buf, n, cap, u16) && append_utf8("\n", buf, n, cap, u16);
    if (mlHeads > 0) {
        ok = ok && append_bold_lit("Головы:", spans, ns, buf, n, cap, u16) &&
             append_snprintf_chunk(buf, n, cap, u16, " %d\n", mlHeads);
    }
    if (ok && mlLate > 0) {
        ok = append_bold_lit("Подголовники:", spans, ns, buf, n, cap, u16) &&
             append_snprintf_chunk(buf, n, cap, u16, " %d\n", mlLate);
    }
    if (ok && mlHearts > 0) {
        ok = append_bold_lit("Тело:", spans, ns, buf, n, cap, u16) &&
             append_snprintf_chunk(buf, n, cap, u16, " %d\n", mlHearts);
    }
    if (ok && tailsUi && mlTails > 0) {
        ok = append_bold_lit("Хвосты:", spans, ns, buf, n, cap, u16) &&
             append_snprintf_chunk(buf, n, cap, u16, " %d\n", mlTails);
    }
}

/** После объёмов: профиль построчно, блоки через «--------»; реле только если rules не пустой. */
static void appendRectificationProfileHumanReadable(char* buf, size_t& n, size_t cap, size_t& u16, VkBoldSpan* spans,
                                                    int& ns, bool& ok) {
    if (!ok) {
        return;
    }
    ok = ok && append_utf8("--------\n", buf, n, cap, u16);
    static const char kProf[] = "Профиль:";
    ProfileService* ps = ProfileService::getInstance();
    if (ps == nullptr) {
        ok = append_bold_lit(kProf, spans, ns, buf, n, cap, u16) && append_utf8("\n", buf, n, cap, u16) &&
             append_utf8("(ProfileService недоступен)\n", buf, n, cap, u16);
        return;
    }
    const String pid = ps->getActiveProfileId();
    if (pid.isEmpty()) {
        ok = append_bold_lit(kProf, spans, ns, buf, n, cap, u16) && append_utf8("\n", buf, n, cap, u16) &&
             append_utf8("(активный профиль не выбран)\n", buf, n, cap, u16);
        return;
    }

    String displayName = pid;
    for (const auto& meta : ps->getProfileList()) {
        if (meta.id == pid) {
            displayName = meta.name;
            break;
        }
    }

    ok = append_bold_lit(kProf, spans, ns, buf, n, cap, u16) && append_utf8("\n", buf, n, cap, u16);
    if (!ok) {
        return;
    }
    ok = append_snprintf_chunk(buf, n, cap, u16, "%s  (id: %s)\n", displayName.c_str(), pid.c_str());

    String content;
    if (!ok || !ps->getProfileContent(pid, content) || content.isEmpty()) {
        ok = ok && append_utf8("(нет файла профиля)\n", buf, n, cap, u16);
        return;
    }

    g_profileJsonAlloc.reset();
    JsonDocument doc(&g_profileJsonAlloc);
    const DeserializationError err = deserializeJson(doc, content.c_str(), content.length());
    if (err) {
        ok = ok && append_utf8("(ошибка разбора JSON профиля)\n", buf, n, cap, u16);
        return;
    }
    redactSecretsInJson(doc.as<JsonVariant>());

    bool any = false;
    if (doc["ssvcSettings"].is<JsonObject>()) {
        const JsonObject s = doc["ssvcSettings"].as<JsonObject>();
        if (s.size() > 0) {
            any = true;
            ok = ok && append_utf8("--------\n", buf, n, cap, u16) && append_utf8("Настройки SSVC\n", buf, n, cap, u16);
            appendJsonObjectFlatLines(s, buf, n, cap, u16, ok);
        }
    }

    if (doc["alarmThresholds"].is<JsonObject>()) {
        const JsonObject a = doc["alarmThresholds"].as<JsonObject>();
        if (a.size() > 0) {
            any = true;
            ok = ok && append_utf8("--------\n", buf, n, cap, u16) && append_utf8("Пороги тревог\n", buf, n, cap, u16);
            appendJsonObjectFlatLines(a, buf, n, cap, u16, ok);
        }
    }

    if (openConnectRelayRulesHasRules(doc["openConnectRelayRules"])) {
        any = true;
        ok = ok && append_utf8("--------\n", buf, n, cap, u16) && append_utf8("Правила реле\n", buf, n, cap, u16);
        appendJsonObjectFlatLines(doc["openConnectRelayRules"].as<JsonObject>(), buf, n, cap, u16, ok);
    }

    if (!any) {
        ok = ok && append_utf8("(нет блоков SSVC / порогов / реле для вывода)\n", buf, n, cap, u16);
    }
}

}  // namespace

VkMessengerClient& VkMessengerClient::instance() {
    static VkMessengerClient inst;
    return inst;
}

VkMessengerClient::VkMessengerClient() = default;

VkMessengerClient::~VkMessengerClient() {
    shutoff();
}

void VkMessengerClient::reloadSettingsSnapshot() {
    if (!_settingsService) {
        return;
    }
    _settingsService->read([&](const VkSettings& s) {
        strncpy(_token, s.accessToken.c_str(), sizeof(_token) - 1);
        _token[sizeof(_token) - 1] = '\0';
        strncpy(_apiVer, s.apiVersion.isEmpty() ? "5.199" : s.apiVersion.c_str(), sizeof(_apiVer) - 1);
        _apiVer[sizeof(_apiVer) - 1] = '\0';
        strncpy(_groupId, s.groupId.c_str(), sizeof(_groupId) - 1);
        _groupId[sizeof(_groupId) - 1] = '\0';
        strncpy(_keyboardBase, s.keyboardBaseUrl.c_str(), sizeof(_keyboardBase) - 1);
        _keyboardBase[sizeof(_keyboardBase) - 1] = '\0';
        _liveEnabled = s.liveEnabled;
        _alertsEnabled = s.alertsEnabled;
        _summaryEnabled = s.summaryEnabled;
        _wallPostEnabled = s.wallPostEnabled;
        _livePeer = parsePeerId(s.livePeerId);
        _alertsPeer = parsePeerId(s.alertsPeerId);
        _summaryPeer = parsePeerId(s.summaryPeerId);
    });
}

bool VkMessengerClient::vkFormPost(const char* method, const char* formBody, char* respBuf, size_t respCap) {
    if (!WiFi.isConnected() || _token[0] == '\0') {
        return false;
    }
    char url[112];
    snprintf(url, sizeof(url), "https://api.vk.com/method/%s", method);

    // Static client: large object; vkFormPost is only called with _httpMutex held from VkMessenger task.
    static HTTPClient http;
    http.end();
    http.setTimeout(12000);
    if (!http.begin(url)) {
        ESP_LOGE(TAG, "http.begin failed");
        return false;
    }
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    const int code = http.POST(formBody);
    respBuf[0] = '\0';
    if (code == HTTP_CODE_OK) {
        const String payload = http.getString();
        if (payload.length() > 0) {
            strncpy(respBuf, payload.c_str(), respCap - 1);
            respBuf[respCap - 1] = '\0';
        }
    } else {
        ESP_LOGW(TAG, "VK HTTP code=%d", code);
    }
    http.end();
    return code == HTTP_CODE_OK && respBuf[0] != '\0';
}

bool VkMessengerClient::sendMessageToPeer(const int64_t peer, const char* text, const char* keyboardJson,
                                          const char* formatDataJson, uint32_t* outMessageId) {
    if (peer == 0 || text == nullptr || _token[0] == '\0') {
        return false;
    }
    char* form = _formBuf;
    size_t pos = 0;
    appendFormKV("access_token", _token, form, &pos, kFormCap);
    appendFormKV("v", _apiVer, form, &pos, kFormCap);
    char peerStr[22];
    snprintf(peerStr, sizeof(peerStr), "%lld", static_cast<long long>(peer));
    appendFormKV("peer_id", peerStr, form, &pos, kFormCap);
    char rnd[16];
    snprintf(rnd, sizeof(rnd), "%ld", static_cast<long>(esp_random() & 0x7FFFFFFF));
    appendFormKV("random_id", rnd, form, &pos, kFormCap);
    appendFormKV("message", text, form, &pos, kFormCap);
    if (keyboardJson != nullptr && keyboardJson[0] != '\0') {
        appendFormKV("keyboard", keyboardJson, form, &pos, kFormCap);
    }
    if (formatDataJson != nullptr && formatDataJson[0] != '\0') {
        appendFormKV("format_data", formatDataJson, form, &pos, kFormCap);
    }

    char* resp = _respBuf;
    if (!vkFormPost("messages.send", form, resp, kRespCap)) {
        return false;
    }
    JsonDocument doc;
    if (deserializeJson(doc, resp) != DeserializationError::Ok) {
        ESP_LOGE(TAG, "send JSON parse fail");
        return false;
    }
    if (doc["error"].is<JsonObject>()) {
        const int ec = doc["error"]["error_code"] | 0;
        ESP_LOGE(TAG, "VK err %d: %s", ec, doc["error"]["error_msg"].as<const char*>());
        return false;
    }
    if (!doc["response"].is<int>()) {
        return false;
    }
    const int mid = doc["response"].as<int>();
    if (outMessageId != nullptr) {
        *outMessageId = static_cast<uint32_t>(mid);
    }
    return true;
}

bool VkMessengerClient::editLiveMessage(const char* text, const char* keyboardJson, const char* formatDataJson) {
    if (_livePeer == 0 || _liveMessageId == 0 || text == nullptr) {
        return false;
    }
    char* form = _formBuf;
    size_t pos = 0;
    appendFormKV("access_token", _token, form, &pos, kFormCap);
    appendFormKV("v", _apiVer, form, &pos, kFormCap);
    char peerStr[22];
    snprintf(peerStr, sizeof(peerStr), "%lld", static_cast<long long>(_livePeer));
    appendFormKV("peer_id", peerStr, form, &pos, kFormCap);
    char midStr[16];
    snprintf(midStr, sizeof(midStr), "%lu", static_cast<unsigned long>(_liveMessageId));
    appendFormKV("message_id", midStr, form, &pos, kFormCap);
    appendFormKV("message", text, form, &pos, kFormCap);
    if (keyboardJson != nullptr && keyboardJson[0] != '\0') {
        appendFormKV("keyboard", keyboardJson, form, &pos, kFormCap);
    }
    if (formatDataJson != nullptr && formatDataJson[0] != '\0') {
        appendFormKV("format_data", formatDataJson, form, &pos, kFormCap);
    }
    char* resp = _respBuf;
    if (!vkFormPost("messages.edit", form, resp, kRespCap)) {
        return false;
    }
    JsonDocument doc;
    if (deserializeJson(doc, resp) != DeserializationError::Ok) {
        return false;
    }
    if (doc["error"].is<JsonObject>()) {
        const int ec = doc["error"]["error_code"] | 0;
        ESP_LOGW(TAG, "edit err %d — reset live msg id", ec);
        _liveMessageId = 0;
        return false;
    }
    return true;
}

bool VkMessengerClient::postWall(const char* text) {
    if (text == nullptr || _groupId[0] == '\0' || _token[0] == '\0') {
        return false;
    }
    const long gid = strtol(_groupId, nullptr, 10);
    if (gid <= 0) {
        return false;
    }
    char owner[24];
    snprintf(owner, sizeof(owner), "-%ld", gid);

    char* form = _formBuf;
    size_t pos = 0;
    appendFormKV("access_token", _token, form, &pos, kFormCap);
    appendFormKV("v", _apiVer, form, &pos, kFormCap);
    appendFormKV("owner_id", owner, form, &pos, kFormCap);
    appendFormKV("from_group", "1", form, &pos, kFormCap);
    appendFormKV("message", text, form, &pos, kFormCap);

    char* resp = _respBuf;
    if (!vkFormPost("wall.post", form, resp, kRespCap)) {
        return false;
    }
    JsonDocument doc;
    if (deserializeJson(doc, resp) != DeserializationError::Ok) {
        return false;
    }
    if (doc["error"].is<JsonObject>()) {
        ESP_LOGE(TAG, "wall err: %s", doc["error"]["error_msg"].as<const char*>());
        return false;
    }
    return true;
}

void VkMessengerClient::buildKeyboardJson(char* out, const size_t cap) const {
    // Inline keyboard is a VK «chat bot» surface; omit unless user set a URL (parity with example/vk_bot.py).
    if (_keyboardBase[0] == '\0') {
        if (cap > 0) {
            out[0] = '\0';
        }
        return;
    }
    char link[128];
    strncpy(link, _keyboardBase, sizeof(link) - 1);
    link[sizeof(link) - 1] = '\0';
    snprintf(out, cap,
             "{\"one_time\":false,\"inline\":true,\"buttons\":[[{\"action\":{\"type\":\"open_link\",\"link\":\"%s\","
             "\"label\":\"Веб\"},\"color\":\"primary\"}]]}",
             link);
}

void VkMessengerClient::buildLiveStatusText(char* buf, const size_t cap, char* fmtOut, const size_t fmtCap) {
    if (fmtOut != nullptr && fmtCap > 0) {
        fmtOut[0] = '\0';
    }
    if (cap == 0) {
        return;
    }

    VkBoldSpan spans[kVkLiveMaxSpans];
    int ns = 0;
    size_t n = 0;
    size_t u16 = 0;
    buf[0] = '\0';
    bool ok = true;

    static const char kEtap[] = "Этап:";
    static const char kTp1[] = "TP1:";
    static const char kTp2[] = "TP2:";

    ok = append_bold_lit("OpenConnect", spans, ns, buf, n, cap, u16) && append_utf8("\n", buf, n, cap, u16) &&
         append_utf8("--------\n", buf, n, cap, u16);

    if (ok && SsvcConnector::getConnector().uartCommunicationError) {
        ok = append_utf8("Нет связи с SSVC\n", buf, n, cap, u16);
    }

    const RectificationProcess::Metrics& m = RectificationProcess::rectController().getMetrics();
    if (ok && !m.type.empty()) {
        const std::string stageStr = RectificationProcess::translateRectificationStage(m.type);
        ok = append_bold_lit(kEtap, spans, ns, buf, n, cap, u16) &&
             append_snprintf_chunk(buf, n, cap, u16, " %s\n", stageStr.c_str());
    }

    if (ok) {
        ok = append_bold_lit(kTp1, spans, ns, buf, n, cap, u16) &&
             append_snprintf_chunk(buf, n, cap, u16, " %.1f  ", m.common.tp1) &&
             append_bold_lit(kTp2, spans, ns, buf, n, cap, u16) &&
             append_snprintf_chunk(buf, n, cap, u16, " %.1f\n", m.common.tp2);
    }

    if (ok) {
        RectificationProcess& rp = RectificationProcess::rectController();
        appendCollectedMlSection(rp, buf, n, cap, u16, spans, ns, ok);
    }

    SensorDataService* sds = SensorDataService::getInstance();
    if (ok && sds != nullptr) {
        bool sepBeforeSensors = true;
        sds->read([&](const SensorDataState& st) {
            int lines = 0;
            for (const auto& z : st.readings_by_zone) {
                if (!ok || lines >= 3) {
                    break;
                }
                const std::string zn =
                    SensorZoneHelper::translateZone(SensorZoneHelper::toString(z.first));
                for (const auto& pr : z.second) {
                    if (!ok || lines >= 3) {
                        break;
                    }
                    if (sepBeforeSensors) {
                        ok = ok && append_utf8("--------\n", buf, n, cap, u16);
                        sepBeforeSensors = false;
                    }
                    ok = append_bold_lit(zn.c_str(), spans, ns, buf, n, cap, u16) &&
                         append_utf8(" ", buf, n, cap, u16) &&
                         append_snprintf_chunk(buf, n, cap, u16, "%s: %.1f\n", sensorIdSuffix(pr.first), pr.second);
                    lines++;
                }
            }
        });
    }

    if (!ok || n == 0) {
        snprintf(buf, cap, "OpenConnect\nнет данных");
        ns = 0;
    }

    if (fmtOut != nullptr && fmtCap > 0) {
        serializeVkFormatData(spans, ns, fmtOut, fmtCap);
    }
}

void VkMessengerClient::buildRectificationSummary(char* buf, const size_t cap, char* fmtOut,
                                                  const size_t fmtCap) const {
    if (fmtOut != nullptr && fmtCap > 0) {
        fmtOut[0] = '\0';
    }
    if (cap == 0) {
        return;
    }

    VkBoldSpan spans[kVkLiveMaxSpans];
    int ns = 0;
    size_t n = 0;
    size_t u16 = 0;
    buf[0] = '\0';
    bool ok = true;

    ok = append_bold_lit("OpenConnect", spans, ns, buf, n, cap, u16) && append_utf8("\n", buf, n, cap, u16) &&
         append_utf8("--------\n", buf, n, cap, u16) && append_utf8("Ректификация завершена\n", buf, n, cap, u16);

    RectificationProcess& rp = RectificationProcess::rectController();
    const RectificationProcess::Metrics& m = rp.getMetrics();

    static const char kNach[] = "Начало:";
    static const char kKon[] = "Конец:";
    ok = ok && append_bold_lit(kNach, spans, ns, buf, n, cap, u16) &&
         append_snprintf_chunk(buf, n, cap, u16, " %s\n", rp.getRectificationStartTime()) &&
         append_bold_lit(kKon, spans, ns, buf, n, cap, u16) &&
         append_snprintf_chunk(buf, n, cap, u16, " %s\n", rp.getRectificationEndTime());

    static const char kEtap[] = "Этап:";
    static const char kTp1[] = "TP1:";
    static const char kTp2[] = "TP2:";
    const char* stageLabel = m.type.empty() ? "-" : RectificationProcess::translateRectificationStage(m.type).c_str();
    ok = ok && append_bold_lit(kEtap, spans, ns, buf, n, cap, u16) &&
         append_snprintf_chunk(buf, n, cap, u16, " %s\n", stageLabel) &&
         append_bold_lit(kTp1, spans, ns, buf, n, cap, u16) &&
         append_snprintf_chunk(buf, n, cap, u16, " %.1f  ", m.common.tp1) &&
         append_bold_lit(kTp2, spans, ns, buf, n, cap, u16) &&
         append_snprintf_chunk(buf, n, cap, u16, " %.1f\n", m.common.tp2);

    ok = ok && append_snprintf_chunk(buf, n, cap, u16, "Остановок: %u\n", static_cast<unsigned>(m.stops));

    appendCollectedMlSection(rp, buf, n, cap, u16, spans, ns, ok);

    appendRectificationProfileHumanReadable(buf, n, cap, u16, spans, ns, ok);

    static const char kVer[] = "Версии:";
    ok = ok && append_bold_lit(kVer, spans, ns, buf, n, cap, u16) && append_utf8("\n", buf, n, cap, u16);
    ok = ok && append_snprintf_chunk(buf, n, cap, u16, "Прошивка OpenConnect: %s\n", APP_VERSION);
    ok = ok && append_snprintf_chunk(buf, n, cap, u16, "API ВКонтакте: %s\n", _apiVer);
    const std::string ssvcVer = SsvcSettings::init().getSsvcVersion();
    ok = ok && append_snprintf_chunk(buf, n, cap, u16, "Прошивка SSVC: %s\n",
                                     ssvcVer.empty() ? "-" : ssvcVer.c_str());
    const float ssvcApi = SsvcSettings::init().getSsvcApiVersion();
    if (ok && ssvcApi > 0.0f) {
        ok = append_snprintf_chunk(buf, n, cap, u16, "API SSVC (контроллер): %.2f\n", static_cast<double>(ssvcApi));
    } else if (ok) {
        ok = append_utf8("API SSVC (контроллер): -\n", buf, n, cap, u16);
    }

    if (!ok || n == 0) {
        snprintf(buf, cap,
                 "OpenConnect\nРектификация завершена\nНачало: %s\nКонец: %s\nTP1: %.1f TP2: %.1f\n",
                 rp.getRectificationStartTime(), rp.getRectificationEndTime(), m.common.tp1, m.common.tp2);
        ns = 0;
    }

    if (fmtOut != nullptr && fmtCap > 0) {
        serializeVkFormatData(spans, ns, fmtOut, fmtCap);
    }
}

bool VkMessengerClient::tryEnqueueAlert(const char* utf8Line) {
    if (_alertQueue == nullptr || utf8Line == nullptr) {
        return false;
    }
    VkAlertMsg msg{};
    strncpy(msg.line, utf8Line, sizeof(msg.line) - 1);
    return xQueueSend(static_cast<QueueHandle_t>(_alertQueue), &msg, 0) == pdTRUE;
}

void VkMessengerClient::runLoop() {
    reloadSettingsSnapshot();

    VkAlertMsg alert{};
    int alertBudget = 4;
    while (alertBudget-- > 0 && xQueueReceive(static_cast<QueueHandle_t>(_alertQueue), &alert, 0) == pdTRUE) {
        if (_alertsEnabled && _alertsPeer != 0 && _token[0] != '\0') {
            MutexLocker lock(_httpMutex);
            uint32_t mid = 0;
            sendMessageToPeer(_alertsPeer, alert.line, nullptr, nullptr, &mid);
        }
    }

    const RectificationTypes::ProcessState cur = RectificationProcess::rectController().getProcessState();
    if (_lastRectState != RectificationTypes::ProcessState::FINISHED &&
        cur == RectificationTypes::ProcessState::FINISHED) {
        _summaryFmtScratch[0] = '\0';
        buildRectificationSummary(_summaryBuf, kSummaryCap, _summaryFmtScratch, kFormatDataCap);
        if (_token[0] != '\0') {
            MutexLocker lock(_httpMutex);
            if (_summaryEnabled && _summaryPeer != 0) {
                uint32_t mid = 0;
                const char* sumFmt = (_summaryFmtScratch[0] != '\0') ? _summaryFmtScratch : nullptr;
                sendMessageToPeer(_summaryPeer, _summaryBuf, nullptr, sumFmt, &mid);
            }
            if (_wallPostEnabled) {
                postWall(_summaryBuf);
            }
        }
    }
    _lastRectState = cur;

    static int tick = 0;
    if (++tick < kLivePeriodTicks) {
        return;
    }
    tick = 0;

    if (!_liveEnabled || _livePeer == 0 || _token[0] == '\0') {
        return;
    }

    buildLiveStatusText(_liveBodyBuf, kLiveBodyCap, _formatDataBuf, sizeof(_formatDataBuf));
    const char* kbJson = nullptr;
    buildKeyboardJson(_kbBuf, kKbCap);
    if (_kbBuf[0] != '\0') {
        kbJson = _kbBuf;
    }
    const char* fmtJson = (_formatDataBuf[0] != '\0') ? _formatDataBuf : nullptr;

    MutexLocker lock(_httpMutex);
    if (_liveMessageId == 0) {
        uint32_t mid = 0;
        if (sendMessageToPeer(_livePeer, _liveBodyBuf, kbJson, fmtJson, &mid)) {
            _liveMessageId = mid;
        }
    } else {
        if (!editLiveMessage(_liveBodyBuf, kbJson, fmtJson)) {
            uint32_t mid = 0;
            if (sendMessageToPeer(_livePeer, _liveBodyBuf, kbJson, fmtJson, &mid)) {
                _liveMessageId = mid;
            }
        }
    }
}

void VkMessengerClient::workerTask(void* param) {
    auto* self = static_cast<VkMessengerClient*>(param);
    while (self->_initialized) {
        vTaskDelay(pdMS_TO_TICKS(500));
        if (!SsvcOpenConnect::getInstance().isOnline()) {
            continue;
        }
        if (self->_httpMutex == nullptr) {
            continue;
        }
        self->runLoop();
    }
    self->_taskHandle = nullptr;
    vTaskDelete(nullptr);
}

bool VkMessengerClient::init(VkSettingsService* settingsService) {
    if (_initialized) {
        return true;
    }
    _settingsService = settingsService;
    if (_settingsService == nullptr) {
        ESP_LOGE(TAG, "VkSettingsService null");
        return false;
    }

    vTaskDelay(pdMS_TO_TICKS(2000));
    while (!SsvcOpenConnect::getInstance().isOnline()) {
        ESP_LOGD(TAG, "wait net");
        vTaskDelay(pdMS_TO_TICKS(3000));
    }

    _httpMutex = xSemaphoreCreateMutex();
    _alertQueue = xQueueCreate(kAlertQueueDepth, sizeof(VkAlertMsg));
    if (_httpMutex == nullptr || _alertQueue == nullptr) {
        ESP_LOGE(TAG, "mutex/queue alloc failed");
        return false;
    }

    reloadSettingsSnapshot();
    _lastRectState = RectificationProcess::rectController().getProcessState();
    _initialized = true;

    // HTTPS/TLS + ArduinoJson + long VK form body need more than default stack (stack canary on 6144).
    constexpr uint32_t kStackWords = 10240;
    if (xTaskCreatePinnedToCore(workerTask, "VkMessenger", kStackWords, this, tskIDLE_PRIORITY + 1, &_taskHandle, 1) !=
        pdPASS) {
        ESP_LOGE(TAG, "task create failed");
        _initialized = false;
        vQueueDelete(static_cast<QueueHandle_t>(_alertQueue));
        _alertQueue = nullptr;
        vSemaphoreDelete(_httpMutex);
        _httpMutex = nullptr;
        return false;
    }

    ESP_LOGI(TAG, "VK messenger started");
    return true;
}

void VkMessengerClient::shutoff() {
    if (_taskHandle == nullptr && _alertQueue == nullptr && _httpMutex == nullptr) {
        _initialized = false;
        return;
    }

    _initialized = false;
    for (int i = 0; i < 60 && _taskHandle != nullptr; ++i) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    if (_taskHandle != nullptr) {
        vTaskDelete(_taskHandle);
        _taskHandle = nullptr;
    }
    if (_alertQueue != nullptr) {
        vQueueDelete(static_cast<QueueHandle_t>(_alertQueue));
        _alertQueue = nullptr;
    }
    if (_httpMutex != nullptr) {
        vSemaphoreDelete(_httpMutex);
        _httpMutex = nullptr;
    }
    _liveMessageId = 0;
    _token[0] = '\0';
    _settingsService = nullptr;
}
