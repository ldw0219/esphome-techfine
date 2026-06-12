#include "pipsolar_select.h"
#include "esphome/core/log.h"

namespace esphome {
namespace pipsolar {

static const char *const TAG = "pipsolar.select";

void PipsolarSelect::dump_config() {
  LOG_SELECT("", "Pipsolar Select", this);
}

void PipsolarSelect::control(const std::string &value) {
  ESP_LOGD(TAG, "Select: got option: %s", value.c_str());

  auto it = this->mapping_.find(value);
  if (it != this->mapping_.end()) {
    const std::string &command = it->second;
    ESP_LOGD(TAG, "Select: mapped option %s, sending command: %s", value.c_str(), command.c_str());

    if (this->parent_ != nullptr) {
      this->parent_->queue_command(command);
    } else {
      ESP_LOGW(TAG, "Select: Parent component is null!");
      return;
    }

    if (this->optimistic_) {
      this->publish_state(value);
    }
  } else {
    ESP_LOGW(TAG, "Select: could not find option %s in mapping", value.c_str());
  }
}

void PipsolarSelect::map_and_publish(const std::string &value) {
  ESP_LOGD(TAG, "Select: got raw value: %s", value.c_str());

  auto it = this->status_mapping_.find(value);
  if (it != this->status_mapping_.end()) {
    const std::string &friendly_value = it->second;
    ESP_LOGD(TAG, "mapped raw value %s to friendly value %s", value.c_str(), friendly_value.c_str());
    this->publish_state(friendly_value);
  } else {
    ESP_LOGD(TAG, "could not find raw value %s in status mapping", value.c_str());
    this->publish_state(value);
  }
}

}  // namespace pipsolar
}  // namespace esphome
