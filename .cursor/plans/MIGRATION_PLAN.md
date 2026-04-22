# Migration Plan: PsychicHttp → ESPAsyncWebServer

**PR Reference:** https://github.com/ewowi/ESP32-sveltekit/pull/1  
**Target Library:** [ESP32Async/ESPAsyncWebServer v3.10.0](https://github.com/ESP32Async/ESPAsyncWebServer/releases/tag/v3.10.0)  
**Status:** In Progress

## Motivation

1. **Heap usage** — PsychicHttp is heavy on RAM; ESPAsyncWebServer is lighter
2. **Concurrent connections** — PsychicHttp limits connections; ESPAsyncWebServer allows more
3. **Active maintenance** — ESPAsyncWebServer v3.x under active development (Mathieu Carbou)
4. **SSL not needed on webserver** — The only benefit of PsychicHttp (SSL) was never properly implemented; OTA and MQTT handle SSL separately

## Scope

- **In scope:** All C++ backend changes to replace PsychicHttp with ESPAsyncWebServer
- **Out of scope:** MQTT (PsychicMqttClient stays as-is), SSL, UI changes

## Impact Assessment

- **56 files** reference `Psychic` types
- **30 headers**, **26 source files**
- **15 GET**, **8 POST**, **2 OPTIONS** endpoint registrations
- **2 WebSocket handlers** (EventSocket + WebSocketServer template)
- **Key templates:** HttpEndpoint.h, EventEndpoint.h, MqttEndpoint.h, WebSocketServer.h

## API Mapping Quick Reference

| PsychicHttp | ESPAsyncWebServer |
|---|---|
| `PsychicHttpServer` | `AsyncWebServer(80)` |
| `PsychicRequest*` | `AsyncWebServerRequest*` |
| `PsychicJsonResponse` | `AsyncJsonResponse*` |
| `PsychicWebSocketHandler` | `AsyncWebSocket("/path")` |
| `PsychicWebSocketClient*` | `AsyncWebSocketClient*` |
| `PsychicUploadHandler` | Built-in upload callback |
| `request->reply(code)` | `request->send(code)` |
| `response.send()` | `request->send(response)` |
| `client->sendMessage(msg)` | `client->text(msg)` |
| `client->socket()` | `client->id()` |
| `_socket.sendAll(msg)` | `_socket.textAll(msg)` |
| Handler returns `esp_err_t` | Handler returns `void` |

---

## Phase 1: Build Configuration ✅

**Files:** `platformio.ini`

### Tasks
- [x] Remove `lib/PsychicHttp/` folder (done in original PR commit)
- [x] Add ESPAsyncWebServer dependency (done in original PR commit)
- [x] Fix version specifier: `ESP32Async/ESPAsyncWebServer#v3.10.0` → `ESP32Async/ESPAsyncWebServer @ ^3.10.0`

### Notes
- v3.10.0 was released Feb 14 2026, confirmed on [GitHub releases](https://github.com/ESP32Async/ESPAsyncWebServer/releases/tag/v3.10.0)
- `PsychicMqttClient` stays — it will pull its own PsychicHttp dependency internally
- `lib_compat_mode = strict` is already set, AsyncTCP will auto-resolve

---

## Phase 2: Security Layer ✅

**Files:** `SecurityManager.h`, `SecuritySettingsService.h`, `SecuritySettingsService.cpp`

### Why first?
Every endpoint uses `wrapRequest()`, `wrapCallback()`, `filterRequest()` from SecurityManager. Nothing compiles without this.

### Tasks
- [x] Define new callback typedefs replacing Psychic types:
  - `ArHttpRequestCallback` = `std::function<void(AsyncWebServerRequest*)>`
  - `ArJsonRequestCallback` = `std::function<void(AsyncWebServerRequest*, JsonVariant&)>`
  - `ArRequestFilterFunc` = `std::function<bool(AsyncWebServerRequest*)>`
- [x] Update `SecurityManager.h` — change all virtual method signatures
- [x] Update `SecuritySettingsService.h` — change member types and method signatures
- [x] Update `SecuritySettingsService.cpp`:
  - `authenticateRequest()` — `request->header()` → `request->getHeader()->value()`
  - `filterRequest()` — removed PsychicHttp bogus request hack and `loadParams()`
  - `wrapRequest()` — returns void, uses `request->send(401)`
  - `wrapCallback()` — same pattern
  - `generateToken()` — uses `AsyncJsonResponse*`, added `hasParam` guard

### Key changes
```
PsychicRequest* → AsyncWebServerRequest*
request->reply(401) → request->send(401); return;
request->header("name") → request->getHeader("name")->value()
request->getParam("name")->value() → same API
PsychicJsonResponse → AsyncJsonResponse*
```

---

## Phase 3: Core Templates (partial ✅)

**Files:** `HttpEndpoint.h`, `EventEndpoint.h`, `WebSocketServer.h`

### Why second?
These template classes define the patterns used by ALL service classes.

### Tasks
- [x] `HttpEndpoint.h`:
  - Replaced `PsychicHttpServer*` with `AsyncWebServer*`
  - GET handler: `AsyncJsonResponse*` with `response->setLength()` before send
  - POST handler: `AsyncCallbackJsonWebHandler` + `addHandler()` instead of `_server->on()` for JSON
  - All handler lambdas return `void`
- [ ] `EventEndpoint.h`:
  - Update to use new SecurityManager typedefs
- [ ] `WebSocketServer.h`:
  - Replace `PsychicWebSocketHandler` with `AsyncWebSocket`
  - Replace `onOpen/onClose/onFrame` with unified `onEvent` callback
  - Replace `client->socket()` with `client->id()`
  - Replace `client->sendMessage()` with `client->text()`
  - Replace `_webSocket.sendAll()` with `_webSocket.textAll()`
  - Remove `setFilter()` — authentication needs different approach with AsyncWebSocket

---

## Phase 4: Core Server & Main

**Files:** `ESP32SvelteKit.h`, `ESP32SvelteKit.cpp`, `src/main.cpp`

### Tasks
- [ ] `src/main.cpp`:
  - Replace `#include <PsychicHttpServer.h>` with `#include <ESPAsyncWebServer.h>`
  - Replace `PsychicHttpServer server;` with `AsyncWebServer server(80);`
- [ ] `ESP32SvelteKit.h`:
  - Replace `#include <PsychicHttp.h>` with `#include <ESPAsyncWebServer.h>`
  - Replace `PsychicHttpServer*` member and constructor param
  - Remove `_numberEndpoints` (not needed — AsyncWebServer handlers are dynamic)
- [ ] `ESP32SvelteKit.cpp`:
  - Remove `_server->config.max_uri_handlers = _numberEndpoints;`
  - Replace `_server->listen(80)` with `_server->begin()`
  - Migrate EMBED_WWW static serving (replace PsychicWebHandler/PsychicResponse)
  - Migrate non-EMBED_WWW static serving (PsychicFileResponse → request->send(FS,...))
  - Migrate `onNotFound` handler
  - Remove `_server->getClientList()` in sleep callback (different API)

---

## Phase 5: EventSocket (WebSocket)

**Files:** `EventSocket.h`, `EventSocket.cpp`

### This is the most complex part of the migration.

### Tasks
- [ ] `EventSocket.h`:
  - Replace `PsychicWebSocketHandler _socket` with `AsyncWebSocket _socket`
  - Change callback signatures:
    - `onWSOpen(PsychicWebSocketClient*)` → `onWSOpen(AsyncWebSocketClient*)`
    - `onWSClose(PsychicWebSocketClient*)` → `onWSClose(AsyncWebSocketClient*)`
    - `onFrame(PsychicWebSocketRequest*, httpd_ws_frame*)` → `onFrame(AsyncWebSocketClient*, AwsFrameInfo*, uint8_t*, size_t)`
- [ ] `EventSocket.cpp`:
  - Setup: replace `_socket.onOpen/onClose/onFrame` + `_server->on()` with `_socket.onEvent()` + `_server->addHandler()`
  - `client->socket()` → `client->id()`
  - `HTTPD_WS_TYPE_TEXT/BINARY` → `WS_TEXT/WS_BINARY` (via `info->opcode`)
  - `client->sendMessage(type, data, len)` → `client->text(data, len)` / `client->binary(data, len)`
  - `_socket.getClient(id)` → `_socket.client(id)` 
  - `_socket.getClientList().size()` → `_socket.count()`
  - Frame deserialization: data comes directly as `uint8_t*` + `size_t`, not via `frame->payload`

---

## Phase 6: Service Layer (Batch)

**Files:** All service .h/.cpp files in `lib/framework/`

These are mostly mechanical replacements since they follow patterns from HttpEndpoint.

### Files to update:
- [ ] `WiFiStatus.h` / `.cpp`
- [ ] `WiFiScanner.h` / `.cpp`
- [ ] `WiFiSettingsService.h` / `.cpp`
- [ ] `NTPStatus.h` / `.cpp`
- [ ] `NTPSettingsService.h` / `.cpp`
- [ ] `MqttStatus.h` / `.cpp`
- [ ] `MqttSettingsService.h` / `.cpp`
- [ ] `SystemStatus.h` / `.cpp`
- [ ] `SleepService.h` / `.cpp`
- [ ] `APStatus.h` / `.cpp`
- [ ] `APSettingsService.h` / `.cpp`
- [ ] `EthernetStatus.h` / `.cpp`
- [ ] `EthernetSettingsService.h` / `.cpp`
- [ ] `RestartService.h` / `.cpp`
- [ ] `FactoryResetService.h` / `.cpp`
- [ ] `AuthenticationService.h` / `.cpp`
- [ ] `FeaturesService.h` / `.cpp`
- [ ] `CoreDump.h` / `.cpp`
- [ ] `DownloadFirmwareService.h` / `.cpp`
- [ ] `MqttEndpoint.h`

### Common replacements per file:
```
#include <PsychicHttp.h>         → #include <ESPAsyncWebServer.h>
PsychicHttpServer*               → AsyncWebServer*
PsychicRequest*                  → AsyncWebServerRequest*
PsychicJsonResponse              → AsyncJsonResponse*
request->reply(code)             → request->send(code)
response.getRoot()               → response->getRoot()
response.send()                  → response->setLength(); request->send(response)
return ESP_OK;                   → return; (or remove return)
esp_err_t handlerFn(...)         → void handlerFn(...)
```

---

## Phase 7: Upload Firmware Service

**Files:** `UploadFirmwareService.h`, `UploadFirmwareService.cpp`

### Tasks
- [ ] Remove `PsychicUploadHandler` — use built-in upload in `server.on()`
- [ ] Change upload handler signature:
  - `index`: `uint64_t` → `size_t`
  - Return type: `esp_err_t` → `void`
- [ ] Remove `_server->maxUploadSize` — validate in handler
- [ ] Change `uploadComplete` and `handleError` return types to `void`
- [ ] Replace `request->reply(code)` with `request->send(code)`
- [ ] Replace `PsychicJsonResponse` with `AsyncJsonResponse`
- [ ] Handle `request->_tempObject` — works the same way

---

## Phase 8: Application Layer

**Files:** `src/LightStateService.h`, `src/LightStateService.cpp`, `src/LightMqttSettingsService.h`, `src/LightMqttSettingsService.cpp`

### Tasks
- [ ] Replace `#include <PsychicHttp.h>` with `#include <ESPAsyncWebServer.h>`
- [ ] Replace `PsychicHttpServer*` with `AsyncWebServer*`
- [ ] These mostly use template abstractions (HttpEndpoint, WebSocketServer) so changes should be minimal

---

## Phase 9: Compile & Fix

### Tasks
- [x] Attempt compilation for `esp32dev` target
- [x] All framework code compiles with zero errors (all .o files generated)
- [ ] Fix Windows toolchain/long-path issue (SCons cannot write .sconsign / ranlib fails)
- [ ] Test basic HTTP endpoints
- [ ] Test WebSocket connectivity
- [ ] Test firmware upload
- [ ] Test authentication flow
- [ ] Memory profiling (compare heap usage before/after)

### Build Status
- **Code compilation: PASS** — All 56+ source files compile without any C++ errors
- **Linking: BLOCKED** — Windows filesystem issue prevents SCons from completing archive/link step
- **Root cause:** `ranlib.exe` cannot find output `.a` file; SCons `.sconsign313.tmp` write fails
- **Not a code issue** — same errors occur with framework-only files (Arduino core)

---

## Risk Register

| Risk | Impact | Mitigation |
|---|---|---|
| PsychicMqttClient pulls conflicting PsychicHttp | Medium | lib_compat_mode=strict isolates deps; verified in build |
| WebSocket behavior differences | High | Thorough testing of EventSocket; v3.10.0 has WS fixes |
| Authentication bypass in migration | Critical | Verify wrapRequest/filterRequest work correctly |
| Memory management (AsyncJsonResponse) | Medium | Framework handles cleanup; verify no leaks |
| Upload handler differences | Medium | Test OTA upload end-to-end |
| handleEarlyDisconnect not wired | Low | Not available in AsyncWebServer upload handler; Update.abort() on error cleans state |

---

## Progress Tracking

| Phase | Status | Files Changed | Notes |
|---|---|---|---|
| 1. Build Config | ✅ Done | 1 | Version specifier fixed |
| 2. Security Layer | ✅ Done | 3 | Typedefs + interface + implementation |
| 3. Core Templates | ✅ Done | 3 | HttpEndpoint, EventEndpoint, WebSocketServer |
| 4. Core Server | ✅ Done | 3 | main.cpp, ESP32SvelteKit.h/cpp |
| 5. EventSocket | ✅ Done | 2 | onEvent unified callback, client->id() |
| 6. Service Layer | ✅ Done | ~30 | Mechanical replacements across all services |
| 7. Upload Service | ✅ Done | 2 | PsychicUploadHandler → built-in upload callback |
| 8. Application | ✅ Done | 4 | LightState, LightMqtt |
| 9. Compile & Fix | 🔧 Partial | — | Code compiles OK; Windows env issue blocks linking |
