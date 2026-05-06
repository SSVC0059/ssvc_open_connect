#ifndef SSVC_OPEN_CONNECT_USER_RELAY_PROFILE_BRIDGE_H
#define SSVC_OPEN_CONNECT_USER_RELAY_PROFILE_BRIDGE_H

#include "core/profiles/IProfileObserver.h"

class UserRelayProfileBridge final : public IProfileObserver {
public:
  static UserRelayProfileBridge& instance();

  const char* getProfileKey() const override { return "openConnectRelayRules"; }
  void onProfileSave(JsonObject& dest) override;
  void onProfileApply(const JsonObject& src) override;

private:
  UserRelayProfileBridge() = default;
};

#endif
