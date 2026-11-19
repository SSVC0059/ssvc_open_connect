/**
 *   ESP32 SvelteKit
 *
 *   A simple, secure and extensible framework for IoT projects for ESP32 platforms
 *   with responsive Sveltekit front-end built with TailwindCSS and DaisyUI.
 *   https://github.com/theelims/ESP32-sveltekit
 *
 *   Copyright (C) 2018 - 2023 rjwats
 *   Copyright (C) 2023 - 2025 theelims
 *
 *   All Rights Reserved. This software may be modified and distributed under
 *   the terms of the LGPL v3 license. See the LICENSE file for details.
 **/

#include <SecuritySettingsService.h>
#include <string.h>

#if FT_ENABLED(FT_SECURITY)

SecuritySettingsService::SecuritySettingsService(AsyncWebServer *server, FS *fs) : _server(server),
                                                                                   _httpEndpoint(SecuritySettings::read, SecuritySettings::update, this, server, SECURITY_SETTINGS_PATH, this),
                                                                                   _fsPersistence(SecuritySettings::read, SecuritySettings::update, this, fs, SECURITY_SETTINGS_FILE),
                                                                                   _jwtHandler(FACTORY_JWT_SECRET)
{
    addUpdateHandler([&](const String &originId)
                     { configureJWTHandler(); },
                     false);
}

void SecuritySettingsService::begin()
{
    _server->on(GENERATE_TOKEN_PATH,
                HTTP_GET,
                wrapRequest(std::bind(&SecuritySettingsService::generateToken, this, std::placeholders::_1),
                            AuthenticationPredicates::IS_ADMIN));

    ESP_LOGV(SVK_TAG, "Registered GET endpoint: %s", GENERATE_TOKEN_PATH);

    _server->on(REFRESH_TOKEN_PATH,
                HTTP_GET,
                wrapRequest(std::bind(&SecuritySettingsService::refreshToken, this, std::placeholders::_1),
                            AuthenticationPredicates::IS_AUTHENTICATED));

    ESP_LOGV(SVK_TAG, "Registered GET endpoint: %s", REFRESH_TOKEN_PATH);

    _httpEndpoint.begin();
    _fsPersistence.readFromFS();
    configureJWTHandler();
}

Authentication SecuritySettingsService::authenticateRequest(AsyncWebServerRequest *request)
{
    if (request->hasHeader(AUTHORIZATION_HEADER))
    {
        String value = request->getHeader(AUTHORIZATION_HEADER)->value();
        if (value.startsWith(AUTHORIZATION_HEADER_PREFIX))
        {
            value = value.substring(AUTHORIZATION_HEADER_PREFIX_LEN);
            return authenticateJWT(value);
        }
    }
    else if (request->hasParam(ACCESS_TOKEN_PARAMATER))
    {
        String value = request->getParam(ACCESS_TOKEN_PARAMATER)->value();
        return authenticateJWT(value);
    }
    return Authentication();
}

void SecuritySettingsService::configureJWTHandler()
{
    _jwtHandler.setSecret(_state.jwtSecret);
}

Authentication SecuritySettingsService::authenticateJWT(String &jwt)
{
    JsonDocument payloadDocument;
    _jwtHandler.parseJWT(jwt, payloadDocument);
    if (payloadDocument.is<JsonObject>())
    {
        JsonObject parsedPayload = payloadDocument.as<JsonObject>();
        String username = parsedPayload["username"];
        for (User _user : _state.users)
        {
            if (_user.username == username && validatePayload(parsedPayload, &_user))
            {
                return Authentication(_user);
            }
        }
    }
    return Authentication();
}

Authentication SecuritySettingsService::authenticate(const String &username, const String &password)
{
    for (User _user : _state.users)
    {
        if (_user.username == username && _user.password == password)
        {
            return Authentication(_user);
        }
    }
    return Authentication();
}

/*
 * Build the identity part of the payload plus, when the clock is trustworthy,
 * the lifetime claims. Without NTP the device time is the 1970 epoch, so iat/exp
 * are omitted on purpose: a token with exp in 1970 would be born expired and
 * sign-in would stay broken until the clock synchronises.
 */
inline void populateJWTPayload(JsonObject &payload, User *user, time_t now)
{
    payload["username"] = user->username;
    payload["admin"] = user->admin;

    if (now > JWT_CLOCK_SYNC_THRESHOLD)
    {
        payload["iat"] = static_cast<long long>(now);
        payload["exp"] = static_cast<long long>(now + JWT_TTL_SECONDS);
    }
}

/*
 * Compare the token payload by subset instead of requiring whole-object equality.
 *
 * Identity claims must match the stored user. iat/exp/nbf are optional: tokens
 * issued before this firmware (and tokens issued before NTP) have no exp and must
 * keep working, otherwise upgrading the firmware would sign everybody out. When
 * present, exp/nbf are enforced only with a synchronised clock — after a reboot
 * before NTP, time() is back in 1970 and checking exp would reject every token.
 */
boolean SecuritySettingsService::validatePayload(JsonObject &parsedPayload, User *user)
{
    JsonVariant username = parsedPayload["username"];
    JsonVariant admin = parsedPayload["admin"];
    if (!username.is<const char *>() || !admin.is<bool>())
    {
        return false;
    }
    if (strcmp(username.as<const char *>(), user->username.c_str()) != 0 ||
        admin.as<bool>() != user->admin)
    {
        return false;
    }

    const time_t now = time(nullptr);
    if (now <= JWT_CLOCK_SYNC_THRESHOLD)
    {
        // Clock not synchronised: lifetime claims cannot be judged, accept.
        return true;
    }

    JsonVariant exp = parsedPayload["exp"];
    if (!exp.isNull())
    {
        if (!exp.is<long long>() || exp.as<long long>() <= static_cast<long long>(now))
        {
            return false;
        }
    }

    JsonVariant nbf = parsedPayload["nbf"];
    if (!nbf.isNull())
    {
        if (!nbf.is<long long>() || nbf.as<long long>() > static_cast<long long>(now))
        {
            return false;
        }
    }

    return true;
}

String SecuritySettingsService::generateJWTAt(User *user, time_t now)
{
    JsonDocument jsonDocument;
    JsonObject payload = jsonDocument.to<JsonObject>();
    populateJWTPayload(payload, user, now);
    return _jwtHandler.buildJWT(payload);
}

String SecuritySettingsService::generateJWT(User *user)
{
    return generateJWTAt(user, time(nullptr));
}

ArRequestFilterFunc SecuritySettingsService::filterRequest(AuthenticationPredicate predicate)
{
    return [this, predicate](AsyncWebServerRequest *request)
    {
        Authentication authentication = authenticateRequest(request);
        return predicate(authentication);
    };
}

ArHttpRequestCallback SecuritySettingsService::wrapRequest(ArHttpRequestCallback onRequest, AuthenticationPredicate predicate)
{
    return [this, onRequest, predicate](AsyncWebServerRequest *request)
    {
        Authentication authentication = authenticateRequest(request);
        if (!predicate(authentication))
        {
            request->send(401);
            return;
        }
        onRequest(request);
    };
}

ArJsonRequestCallback SecuritySettingsService::wrapCallback(ArJsonRequestCallback onRequest, AuthenticationPredicate predicate)
{
    return [this, onRequest, predicate](AsyncWebServerRequest *request, JsonVariant &json)
    {
        Authentication authentication = authenticateRequest(request);
        if (!predicate(authentication))
        {
            request->send(401);
            return;
        }
        onRequest(request, json);
    };
}

void SecuritySettingsService::generateToken(AsyncWebServerRequest *request)
{
    if (!request->hasParam("username"))
    {
        request->send(400);
        return;
    }
    String usernameParam = request->getParam("username")->value();
    for (User _user : _state.users)
    {
        if (_user.username == usernameParam)
        {
            AsyncJsonResponse *response = new AsyncJsonResponse();
            JsonObject root = response->getRoot();
            root["token"] = generateJWT(&_user);
            response->setLength();
            request->send(response);
            return;
        }
    }
    request->send(401);
}

/*
 * GET /rest/refreshToken — issue a new token for any authenticated user.
 *
 * wrapRequest(IS_AUTHENTICATED) has already rejected a bad/expired token with 401,
 * so re-reading the request here only recovers the User behind it. exp is reported
 * from the same instant used to build the token, and is null until the clock is
 * synchronised (the token then carries no exp either).
 */
void SecuritySettingsService::refreshToken(AsyncWebServerRequest *request)
{
    Authentication authentication = authenticateRequest(request);
    if (!authentication.authenticated)
    {
        request->send(401);
        return;
    }

    const time_t now = time(nullptr);

    AsyncJsonResponse *response = new AsyncJsonResponse();
    JsonObject root = response->getRoot();
    root["token"] = generateJWTAt(authentication.user, now);
    if (now > JWT_CLOCK_SYNC_THRESHOLD)
    {
        root["exp"] = static_cast<long long>(now + JWT_TTL_SECONDS);
    }
    else
    {
        root["exp"] = nullptr;
    }
    response->setLength();
    request->send(response);
}

#else

User ADMIN_USER = User(FACTORY_ADMIN_USERNAME, FACTORY_ADMIN_PASSWORD, true);

SecuritySettingsService::SecuritySettingsService(AsyncWebServer *server, FS *fs) : SecurityManager()
{
}
SecuritySettingsService::~SecuritySettingsService()
{
}

ArRequestFilterFunc SecuritySettingsService::filterRequest(AuthenticationPredicate predicate)
{
    return [this, predicate](AsyncWebServerRequest *request)
    {
        return true;
    };
}

Authentication SecuritySettingsService::authenticateRequest(AsyncWebServerRequest *request)
{
    return Authentication(ADMIN_USER);
}

ArHttpRequestCallback SecuritySettingsService::wrapRequest(ArHttpRequestCallback onRequest,
                                                           AuthenticationPredicate predicate)
{
    return onRequest;
}

ArJsonRequestCallback SecuritySettingsService::wrapCallback(ArJsonRequestCallback onRequest,
                                                            AuthenticationPredicate predicate)
{
    return onRequest;
}

#endif
