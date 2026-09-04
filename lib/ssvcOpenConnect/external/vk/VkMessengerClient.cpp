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
#include "core/profiles/ProfileService.h"
#include "core/StatefulServices/VkSettingsService/VkSettingsService.h"
#include "core/StatefulServices/SensorDataService/SensorDataService.h"
#include "core/rectification/RectificationProcess.h"
#include "components/Zone/SensorZone.h"

namespace {

constexpr size_t kAlertMsgMax = 144;
constexpr size_t kAlertQueueDepth = 4;
constexpr int kVkPeriodTicks = 40;  // 40 * 500ms = 20s between live edits and alert sends
constexpr uint32_t kVkProbeCacheOkMs = 60000;
constexpr uint32_t kVkProbeCacheFailMs = 10000;

uint32_t s_vkProbeLastMs = 0;
bool s_vkProbeLastOk = false;

bool probeVkApiNow() {
    if (!WiFi.isConnected()) {
        return false;
    }
    HTTPClient http;
    http.setTimeout(5000);
    constexpr char kProbeUrl[] = "https://api.vk.com/method/utils.getServerTime?v=5.199";
    if (!http.begin(kProbeUrl)) {
        ESP_LOGD("VkMessenger", "VK probe: http.begin failed");
        return false;
    }
    const int code = http.GET();
    if (code != HTTP_CODE_OK) {
        ESP_LOGD("VkMessenger", "VK probe: HTTP %d", code);
        http.end();
        return false;
    }
    const String payload = http.getString();
    http.end();
    return payload.indexOf("\"response\"") >= 0;
}

struct VkAlertMsg {
    char line[kAlertMsgMax];
};

void logVkRespPreview(const char* ctx, const char* resp) {
    if (resp == nullptr || resp[0] == '\0') {
        ESP_LOGW("VkMessenger", "%s: empty response body", ctx);
        return;
    }
    ESP_LOGW("VkMessenger", "%s: %.192s", ctx, resp);
}

/** Best-effort when JSON truncated but body contains VK error object. */
int peekVkErrorCode(const char* resp) {
    if (resp == nullptr) {
        return 0;
    }
    const char* p = strstr(resp, "\"error_code\":");
    if (p == nullptr) {
        p = strstr(resp, "\"error_code\" :");
        if (p == nullptr) {
            return 0;
        }
        p += 13;
        while (*p == ' ') {
            ++p;
        }
    } else {
        p += 13;
    }
    return atoi(p);
}

bool vkErrorIsRateOrFlood(int ec) {
    switch (ec) {
        case 6:    // Too many requests per second
        case 9:    // Flood control
        case 984:  // Spam restriction
            return true;
        default:
            return false;
    }
}

/** Message id invalid — allow messages.send on a later cycle. Never for flood (9, 6, 984). */
bool vkEditErrorNeedsNewMessage(int ec) {
    if (vkErrorIsRateOrFlood(ec)) {
        return false;
    }
    switch (ec) {
        case 909:  // too old to edit
        case 924:  // message not found (legacy)
            return true;
        default:
            return false;
    }
}

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

static int collectedMlFor(const std::map<RectificationTypes::Stage, int>& volumes,
                          RectificationTypes::Stage stage) {
    const auto it = volumes.find(stage);
    return it == volumes.end() ? 0 : it->second;
}

/** «Отобрано, мл:» только если есть ненулевые объёмы; строки с нулём не выводятся. Порядок: головы, подголовники,
 *  тело, хвосты (последнее — только при поддержке хвостов). Перед блоком — «--------». */
static void appendCollectedMlSection(const RectificationProcess::Snapshot& snap, char* buf, size_t& n, size_t cap,
                                     size_t& u16, VkBoldSpan* spans, int& ns, bool& ok) {
    if (!ok) {
        return;
    }
    const bool tailsUi = SsvcSettings::init().isSupportTails();
    const int mlHeads = collectedMlFor(snap.flowVolumeValves, RectificationTypes::Stage::HEADS);
    const int mlLate = collectedMlFor(snap.flowVolumeValves, RectificationTypes::Stage::LATE_HEADS);
    const int mlHearts = collectedMlFor(snap.flowVolumeValves, RectificationTypes::Stage::HEARTS);
    const int mlTails = tailsUi ? collectedMlFor(snap.flowVolumeValves, RectificationTypes::Stage::TAILS) : 0;

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

bool VkMessengerClient::isVkApiReachable() {
    const uint32_t now = millis();
    const uint32_t cacheMs = s_vkProbeLastOk ? kVkProbeCacheOkMs : kVkProbeCacheFailMs;
    if (s_vkProbeLastMs != 0 && (now - s_vkProbeLastMs) < cacheMs) {
        return s_vkProbeLastOk;
    }
    s_vkProbeLastOk = probeVkApiNow();
    s_vkProbeLastMs = now;
    if (!s_vkProbeLastOk) {
        ESP_LOGI(TAG, "VK API probe failed (cached %u ms)", cacheMs);
    } else {
        ESP_LOGD(TAG, "VK API reachable");
    }
    return s_vkProbeLastOk;
}

void VkMessengerClient::applyLiveVkBackoff(const uint32_t durationMs) {
    const uint32_t until = millis() + durationMs;
    if (until > _liveVkBackoffUntilMs) {
        _liveVkBackoffUntilMs = until;
    }
    ESP_LOGW(TAG, "live VK backoff %u ms (until %u)", durationMs, _liveVkBackoffUntilMs);
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
        _liveEnabled = s.liveEnabled;
        _alertsEnabled = s.alertsEnabled;
        _summaryEnabled = s.summaryEnabled;
        _wallPostEnabled = s.wallPostEnabled;
        const int64_t newPeer = parsePeerId(s.peerId);
        if (_peer != 0 && newPeer != _peer) {
            _liveMessageId = 0;
            _alertMessageId = 0;
        }
        _peer = newPeer;
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
        ESP_LOGW(TAG, "%s HTTP code=%d", method, code);
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
        ESP_LOGW(TAG, "messages.send transport fail peer=%lld kb=%d fmt=%d", static_cast<long long>(peer),
                 keyboardJson != nullptr && keyboardJson[0] != '\0',
                 formatDataJson != nullptr && formatDataJson[0] != '\0');
        return false;
    }
    JsonDocument doc;
    if (deserializeJson(doc, resp) != DeserializationError::Ok) {
        ESP_LOGE(TAG, "messages.send JSON parse fail peer=%lld", static_cast<long long>(peer));
        logVkRespPreview("messages.send", resp);
        return false;
    }
    if (doc["error"].is<JsonObject>()) {
        const int ec = doc["error"]["error_code"] | 0;
        ESP_LOGE(TAG, "messages.send VK err %d: %s", ec, doc["error"]["error_msg"].as<const char*>());
        if (vkErrorIsRateOrFlood(ec)) {
            applyLiveVkBackoff(ec == 9 ? 60000 : 120000);
        }
        return false;
    }
    if (!doc["response"].is<int>()) {
        ESP_LOGE(TAG, "messages.send bad response type peer=%lld (expected int message id)", static_cast<long long>(peer));
        logVkRespPreview("messages.send", resp);
        return false;
    }
    const int mid = doc["response"].as<int>();
    if (outMessageId != nullptr) {
        *outMessageId = static_cast<uint32_t>(mid);
    }
    ESP_LOGI(TAG, "messages.send ok peer=%lld message_id=%d", static_cast<long long>(peer), mid);
    return true;
}

bool VkMessengerClient::deleteMessage(const int64_t peer, const uint32_t messageId) {
    if (peer == 0 || messageId == 0 || _token[0] == '\0') {
        return false;
    }
    char* form = _formBuf;
    size_t pos = 0;
    appendFormKV("access_token", _token, form, &pos, kFormCap);
    appendFormKV("v", _apiVer, form, &pos, kFormCap);
    char peerStr[22];
    snprintf(peerStr, sizeof(peerStr), "%lld", static_cast<long long>(peer));
    appendFormKV("peer_id", peerStr, form, &pos, kFormCap);
    char midStr[16];
    snprintf(midStr, sizeof(midStr), "%lu", static_cast<unsigned long>(messageId));
    appendFormKV("message_ids", midStr, form, &pos, kFormCap);
    appendFormKV("delete_for_all", "1", form, &pos, kFormCap);

    char* resp = _respBuf;
    if (!vkFormPost("messages.delete", form, resp, kRespCap)) {
        ESP_LOGW(TAG, "messages.delete transport fail peer=%lld mid=%lu", static_cast<long long>(peer),
                 static_cast<unsigned long>(messageId));
        return false;
    }
    JsonDocument doc;
    if (deserializeJson(doc, resp) != DeserializationError::Ok) {
        ESP_LOGW(TAG, "messages.delete JSON parse fail mid=%lu", static_cast<unsigned long>(messageId));
        return false;
    }
    if (doc["error"].is<JsonObject>()) {
        const int ec = doc["error"]["error_code"] | 0;
        ESP_LOGW(TAG, "messages.delete VK err %d mid=%lu: %s", ec, static_cast<unsigned long>(messageId),
                 doc["error"]["error_msg"].as<const char*>());
        if (vkErrorIsRateOrFlood(ec)) {
            applyLiveVkBackoff(ec == 9 ? 60000 : 120000);
        }
        return false;
    }
    ESP_LOGI(TAG, "messages.delete ok peer=%lld mid=%lu", static_cast<long long>(peer),
             static_cast<unsigned long>(messageId));
    return true;
}

bool VkMessengerClient::editLiveMessage(const char* text, const char* keyboardJson, const char* formatDataJson) {
    if (_peer == 0 || _liveMessageId == 0 || text == nullptr) {
        ESP_LOGW(TAG, "live edit skipped (peer=%lld stored_id=%lu text=%p)", static_cast<long long>(_peer),
                 static_cast<unsigned long>(_liveMessageId), static_cast<const void*>(text));
        return false;
    }
    char* form = _formBuf;
    size_t pos = 0;
    appendFormKV("access_token", _token, form, &pos, kFormCap);
    appendFormKV("v", _apiVer, form, &pos, kFormCap);
    char peerStr[22];
    snprintf(peerStr, sizeof(peerStr), "%lld", static_cast<long long>(_peer));
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
        ESP_LOGW(TAG, "messages.edit transport fail mid=%lu fmt=%d kb=%d", static_cast<unsigned long>(_liveMessageId),
                 formatDataJson != nullptr && formatDataJson[0] != '\0',
                 keyboardJson != nullptr && keyboardJson[0] != '\0');
        return false;
    }
    JsonDocument doc;
    const DeserializationError jerr = deserializeJson(doc, resp);
    if (jerr != DeserializationError::Ok) {
        const int ecPeek = peekVkErrorCode(resp);
        ESP_LOGW(TAG, "messages.edit JSON parse fail mid=%lu peek_err=%d", static_cast<unsigned long>(_liveMessageId),
                 ecPeek);
        logVkRespPreview("messages.edit", resp);
        if (vkErrorIsRateOrFlood(ecPeek)) {
            applyLiveVkBackoff(ecPeek == 9 ? 60000 : 120000);
            _liveEditRetryAfterBackoff = true;
        }
        return false;
    }
    if (doc["error"].is<JsonObject>()) {
        const int ec = doc["error"]["error_code"] | 0;
        ESP_LOGW(TAG, "messages.edit VK err %d mid=%lu: %s", ec, static_cast<unsigned long>(_liveMessageId),
                 doc["error"]["error_msg"].as<const char*>());
        if (vkErrorIsRateOrFlood(ec)) {
            ESP_LOGW(TAG, "messages.edit rate/flood (ec=%d): keep mid=%lu, retry edit after backoff",
                     ec, static_cast<unsigned long>(_liveMessageId));
            applyLiveVkBackoff(ec == 9 ? 60000 : 120000);
            _liveEditRetryAfterBackoff = true;
            return false;
        }
        if (vkEditErrorNeedsNewMessage(ec)) {
            ESP_LOGW(TAG, "messages.edit ec=%d: clearing stored mid=%lu (will send new on next live cycle)", ec,
                     static_cast<unsigned long>(_liveMessageId));
            _liveMessageId = 0;
            _liveEditRetryAfterBackoff = false;
        }
        return false;
    }
    _liveEditRetryAfterBackoff = false;
    ESP_LOGI(TAG, "messages.edit ok mid=%lu peer=%lld text_len=%u", static_cast<unsigned long>(_liveMessageId),
             static_cast<long long>(_peer), static_cast<unsigned>(strlen(text)));
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

void VkMessengerClient::flushAlertMessageIfDue(const uint32_t nowMs) {
    if (!_alertsEnabled || _peer == 0 || _token[0] == '\0' || !_hasPendingAlert) {
        return;
    }
    if (_liveVkBackoffUntilMs != 0 && nowMs < _liveVkBackoffUntilMs) {
        ESP_LOGD(TAG, "alert cycle skipped (VK backoff, %u ms left)", _liveVkBackoffUntilMs - nowMs);
        return;
    }

    MutexLocker lock(_httpMutex);
    if (_alertMessageId != 0) {
        const uint32_t oldMid = _alertMessageId;
        if (deleteMessage(_peer, oldMid)) {
            _alertMessageId = 0;
        } else {
            ESP_LOGW(TAG, "alert delete failed mid=%lu (will try send anyway)", static_cast<unsigned long>(oldMid));
            _alertMessageId = 0;
        }
    }
    uint32_t mid = 0;
    if (sendMessageToPeer(_peer, _pendingAlertLine, nullptr, nullptr, &mid)) {
        _alertMessageId = mid;
        _hasPendingAlert = false;
        ESP_LOGI(TAG, "alert sent peer=%lld mid=%lu: %.80s", static_cast<long long>(_peer),
                 static_cast<unsigned long>(mid), _pendingAlertLine);
    } else {
        ESP_LOGW(TAG, "alert send failed peer=%lld: %.80s", static_cast<long long>(_peer), _pendingAlertLine);
    }
}

void VkMessengerClient::runLiveCycle(const uint32_t nowMs) {
    if (!_liveEnabled || _peer == 0 || _token[0] == '\0') {
        return;
    }
    if (_liveVkBackoffUntilMs != 0 && nowMs < _liveVkBackoffUntilMs) {
        ESP_LOGD(TAG, "live cycle skipped (VK backoff, %u ms left)", _liveVkBackoffUntilMs - nowMs);
        return;
    }

    buildLiveStatusText(_liveBodyBuf, kLiveBodyCap, _formatDataBuf, sizeof(_formatDataBuf));
    const char* fmtJson = (_formatDataBuf[0] != '\0') ? _formatDataBuf : nullptr;
    const size_t bodyLen = strlen(_liveBodyBuf);

    MutexLocker lock(_httpMutex);
    if (_liveMessageId == 0) {
        ESP_LOGI(TAG, "live cycle: send new (peer=%lld body=%u fmt=%d)", static_cast<long long>(_peer),
                 static_cast<unsigned>(bodyLen), fmtJson != nullptr ? 1 : 0);
        uint32_t mid = 0;
        if (sendMessageToPeer(_peer, _liveBodyBuf, nullptr, fmtJson, &mid)) {
            _liveMessageId = mid;
            ESP_LOGI(TAG, "live stored message_id=%lu", static_cast<unsigned long>(_liveMessageId));
        } else {
            ESP_LOGW(TAG, "live send failed, message_id still 0");
        }
    } else {
        ESP_LOGD(TAG, "live cycle: edit mid=%lu body=%u fmt=%d", static_cast<unsigned long>(_liveMessageId),
                 static_cast<unsigned>(bodyLen), fmtJson != nullptr ? 1 : 0);
        const uint32_t editMid = _liveMessageId;
        if (!editLiveMessage(_liveBodyBuf, nullptr, fmtJson)) {
            ESP_LOGW(TAG, "live edit failed mid=%lu (flood: id kept, retry after backoff; 909/924: id cleared)",
                     static_cast<unsigned long>(editMid));
        } else {
            _liveEditRetryAfterBackoff = false;
        }
    }
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

    RectificationProcess::Snapshot snap{};
    RectificationProcess::rectController().getSnapshot(snap);
    const RectificationProcess::Metrics& m = snap.metric;
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
        appendCollectedMlSection(snap, buf, n, cap, u16, spans, ns, ok);
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
        ESP_LOGW(TAG, "live text build fallback (ok=%d len=%u spans=%d fmt_buf=%u) — format_data cleared",
                 ok ? 1 : 0, static_cast<unsigned>(n), ns,
                 static_cast<unsigned>(fmtOut != nullptr && fmtCap > 0 ? fmtCap : 0));
        snprintf(buf, cap, "OpenConnect\nнет данных");
        ns = 0;
    } else {
        ESP_LOGD(TAG, "live text ok len=%u spans=%d has_fmt=%d", static_cast<unsigned>(n), ns,
                 (fmtOut != nullptr && fmtOut[0] != '\0') ? 1 : 0);
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

    RectificationProcess::Snapshot snap{};
    RectificationProcess::rectController().getSnapshot(snap);
    const RectificationProcess::Metrics& m = snap.metric;

    static const char kNach[] = "Начало:";
    static const char kKon[] = "Конец:";
    ok = ok && append_bold_lit(kNach, spans, ns, buf, n, cap, u16) &&
         append_snprintf_chunk(buf, n, cap, u16, " %s\n", snap.startTime) &&
         append_bold_lit(kKon, spans, ns, buf, n, cap, u16) &&
         append_snprintf_chunk(buf, n, cap, u16, " %s\n", snap.endTime);

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

    appendCollectedMlSection(snap, buf, n, cap, u16, spans, ns, ok);

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
                 snap.startTime, snap.endTime, m.common.tp1, m.common.tp2);
        ns = 0;
    }

    if (fmtOut != nullptr && fmtCap > 0) {
        serializeVkFormatData(spans, ns, fmtOut, fmtCap);
    }
}

bool VkMessengerClient::tryEnqueueAlert(const char* utf8Line) {
    // The queue is allocated once for the singleton lifetime (see init()/shutoff()), so an
    // enqueue from a producer task can never race a vQueueDelete (use-after-free); only gate
    // on the soft enabled flag to avoid accumulating stale alerts while the subsystem is off.
    if (!_initialized || _alertQueue == nullptr || utf8Line == nullptr) {
        return false;
    }
    VkAlertMsg msg{};
    strncpy(msg.line, utf8Line, sizeof(msg.line) - 1);
    const bool ok = xQueueSend(static_cast<QueueHandle_t>(_alertQueue), &msg, 0) == pdTRUE;
    if (!ok) {
        ESP_LOGW(TAG, "alert queue full, dropped: %.80s", utf8Line);
    } else {
        ESP_LOGD(TAG, "alert queued: %.80s", utf8Line);
    }
    return ok;
}

void VkMessengerClient::runLoop() {
    reloadSettingsSnapshot();

    VkAlertMsg alert{};
    while (xQueueReceive(static_cast<QueueHandle_t>(_alertQueue), &alert, 0) == pdTRUE) {
        strncpy(_pendingAlertLine, alert.line, sizeof(_pendingAlertLine) - 1);
        _pendingAlertLine[sizeof(_pendingAlertLine) - 1] = '\0';
        _hasPendingAlert = true;
    }

    RectificationProcess::Snapshot snap{};
    RectificationProcess::rectController().getSnapshot(snap);
    const RectificationTypes::ProcessState cur = snap.state;
    if (_lastRectState != RectificationTypes::ProcessState::FINISHED &&
        cur == RectificationTypes::ProcessState::FINISHED) {
        _summaryFmtScratch[0] = '\0';
        buildRectificationSummary(_summaryBuf, kSummaryCap, _summaryFmtScratch, kFormatDataCap);
        if (_token[0] != '\0') {
            MutexLocker lock(_httpMutex);
            if (_summaryEnabled && _peer != 0) {
                uint32_t mid = 0;
                const char* sumFmt = (_summaryFmtScratch[0] != '\0') ? _summaryFmtScratch : nullptr;
                sendMessageToPeer(_peer, _summaryBuf, nullptr, sumFmt, &mid);
            }
            if (_wallPostEnabled) {
                postWall(_summaryBuf);
            }
        }
    }
    _lastRectState = cur;

    const uint32_t nowMs = millis();

    static int liveTick = 0;
    if (_liveEditRetryAfterBackoff && _liveMessageId != 0 &&
        (_liveVkBackoffUntilMs == 0 || nowMs >= _liveVkBackoffUntilMs)) {
        liveTick = kVkPeriodTicks - 1;
    }
    if (++liveTick >= kVkPeriodTicks) {
        liveTick = 0;
        runLiveCycle(nowMs);
    }

    static int alertTick = 0;
    if (++alertTick >= kVkPeriodTicks) {
        alertTick = 0;
        flushAlertMessageIfDue(nowMs);
    }
}

void VkMessengerClient::workerTask(void* param) {
    auto* self = static_cast<VkMessengerClient*>(param);
    while (self->_initialized) {
        vTaskDelay(pdMS_TO_TICKS(500));
        if (!isVkApiReachable()) {
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

    // Allocate once and keep for the singleton lifetime: alert producers run on other tasks
    // (sensor/re-alarm timers, UART, I2C pollers), so freeing the queue here or in shutoff()
    // would race their enqueue (use-after-free). Just drain stale alerts from a previous
    // disabled period instead.
    if (_httpMutex == nullptr) {
        _httpMutex = xSemaphoreCreateMutex();
    }
    if (_alertQueue == nullptr) {
        _alertQueue = xQueueCreate(kAlertQueueDepth, sizeof(VkAlertMsg));
    }
    if (_httpMutex == nullptr || _alertQueue == nullptr) {
        ESP_LOGE(TAG, "mutex/queue alloc failed");
        return false;
    }
    xQueueReset(static_cast<QueueHandle_t>(_alertQueue));

    reloadSettingsSnapshot();
    RectificationProcess::Snapshot snap{};
    RectificationProcess::rectController().getSnapshot(snap);
    _lastRectState = snap.state;
    _initialized = true;

    // HTTPS/TLS + ArduinoJson + long VK form body need more than default stack (stack canary on 6144).
    constexpr uint32_t kStackWords = 10240;
    if (xTaskCreatePinnedToCore(workerTask, "VkMessenger", kStackWords, this, tskIDLE_PRIORITY + 1, &_taskHandle, 1) !=
        pdPASS) {
        ESP_LOGE(TAG, "task create failed");
        _initialized = false;
        return false;
    }

    ESP_LOGI(TAG, "VK messenger started");
    return true;
}

void VkMessengerClient::shutoff() {
    _initialized = false;

    // Wait briefly for the worker to exit on its own; force-kill only as a last resort
    // (the worker may be inside a 12 s HTTP call — tracked as H8/H9 in the review backlog).
    if (_taskHandle != nullptr) {
        for (int i = 0; i < 60 && _taskHandle != nullptr; ++i) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        if (_taskHandle != nullptr) {
            vTaskDelete(_taskHandle);
            _taskHandle = nullptr;
        }
    }

    // K1: never free _alertQueue/_httpMutex here — producer tasks (sensor/re-alarm timers,
    // UART, I2C pollers) may still be enqueuing concurrently with this teardown; keeping the
    // queue/mutex allocated for the singleton lifetime removes the check-then-use-after-free
    // window on disable. Only drain the queue so stale alerts don't fire on re-enable.
    if (_alertQueue != nullptr) {
        xQueueReset(static_cast<QueueHandle_t>(_alertQueue));
    }

    _liveMessageId = 0;
    _alertMessageId = 0;
    _hasPendingAlert = false;
    _pendingAlertLine[0] = '\0';
    _liveVkBackoffUntilMs = 0;
    _liveEditRetryAfterBackoff = false;
    _token[0] = '\0';
    _settingsService = nullptr;
    ESP_LOGI(TAG, "VK messenger shutoff (live message_id cleared)");
}
