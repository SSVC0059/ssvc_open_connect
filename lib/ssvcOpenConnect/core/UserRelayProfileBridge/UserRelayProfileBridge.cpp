#include "UserRelayProfileBridge.h"
#include "core/RelayRuleEngine/RelayRuleEngine.h"

UserRelayProfileBridge& UserRelayProfileBridge::instance() {
  static UserRelayProfileBridge inst;
  return inst;
}

void UserRelayProfileBridge::onProfileSave(JsonObject& dest) {
  RelayRuleEngine::getInstance().serializeToJson(dest);
}

void UserRelayProfileBridge::onProfileApply(const JsonObject& src) {
  RelayRuleEngine::getInstance().loadRules(src);
}
