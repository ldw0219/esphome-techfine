#include "pipsolar_select.h"
#include "esphome/core/log.h"

namespace esphome {
namespace pipsolar {

static const char *const TAG = "pipsolar.select";

void PipsolarSelect::dump_config() {
  LOG_SELECT("", "Pipsolar Select", this);
  // 移除 this->log_select_options();，ESPhome 没有这个方法
}

void PipsolarSelect::control(const std::string &value) {
  ESP_LOGD(TAG, "Select: got option: %s", value.c_str());
  
  if (this->mapping_.find(value) != this->mapping_.end()) {
    std::string command = this->mapping_[value];
    ESP_LOGD(TAG, "Select: mapped option %s for option %s, sending command: %s", command.c_str(), value.c_str(), command.c_str());
    this->parent_->queue_command(command);
    
    if (this->optimistic_) {
      this->publish_state(value);
    }
  } else {
    ESP_LOGW(TAG, "Select: could not find option %s in mapping", value.c_str());
  }
}

void PipsolarSelect::map_and_publish(const std::string &value) {
  ESP_LOGD(TAG, "Select: got raw value: %s", value.c_str());
  
  if (this->status_mapping_.find(value) != this->status_mapping_.end()) {
    std::string friendly_value = this->status_mapping_[value];
    ESP_LOGD(TAG, "mapped raw value %s to friendly value %s", value.c_str(), friendly_value.c_str());
    this->publish_state(friendly_value);
  } else {
    ESP_LOGD(TAG, "could not find raw value %s in status mapping", value.c_str());
    this->publish_state(value);
  }
}

} // namespace pipsolar
} // namespace esphome
