#ifndef SSVC_OPEN_CONNECT_HARDWARE_FAULT_LOG_SUBSCRIBER_H
#define SSVC_OPEN_CONNECT_HARDWARE_FAULT_LOG_SUBSCRIBER_H

#include "core/IAlarmSubscriber/IAlarmSubscriber.h"

/** Stub: logs HARDWARE_FAULT events (full handling may live in NotificationSubscriber). */
class HardwareFaultLogSubscriber final : public IAlarmSubscriber {
public:
  HardwareFaultLogSubscriber();
  ~HardwareFaultLogSubscriber() override;
  void onAlarm(const AlarmEvent& event) override;
  void forceResetAlarm() override {}

private:
  static constexpr auto TAG = "HwFaultLog";
};

#endif
