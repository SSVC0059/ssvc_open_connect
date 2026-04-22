#include "HardwareFaultLogSubscriber.h"
#include "core/AlarmMonitor/AlarmMonitor.h"
#include <esp_log.h>

HardwareFaultLogSubscriber::HardwareFaultLogSubscriber() {
  AlarmMonitor::getInstance().subscribe(this);
  ESP_LOGI(TAG, "subscribed to AlarmMonitor (hardware fault log stub)");
}

HardwareFaultLogSubscriber::~HardwareFaultLogSubscriber() {
  AlarmMonitor::getInstance().unsubscribe(this);
}

void HardwareFaultLogSubscriber::onAlarm(const AlarmEvent& event) {
  if (event.source_kind != AlarmSourceKind::HARDWARE_FAULT) {
    return;
  }
  if (event.level == AlarmLevel::NORMAL) {
    ESP_LOGI(TAG, "hardware fault cleared: role=%s addr=0x%02X",
             event.hw_device_role ? event.hw_device_role : "?",
             static_cast<unsigned>(event.hw_i2c_address));
    return;
  }
  ESP_LOGE(TAG, "HARDWARE FAULT: role=%s addr=0x%02X code=%u level=%d",
           event.hw_device_role ? event.hw_device_role : "?",
           static_cast<unsigned>(event.hw_i2c_address),
           static_cast<unsigned>(event.hw_code), static_cast<int>(event.level));
}
