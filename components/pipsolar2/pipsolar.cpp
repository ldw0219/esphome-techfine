#include "pipsolar.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "pipsolar_select.h"

namespace esphome {
namespace pipsolar {

static const char *const TAG = "pipsolar";

void Pipsolar::setup() {
  this->state_ = STATE_IDLE;
  this->command_start_millis_ = 0;
}

void Pipsolar::empty_uart_buffer_() {
  uint8_t buf[64];
  size_t avail;
  while ((avail = this->available()) > 0) {
    if (!this->read_array(buf, std::min(avail, sizeof(buf)))) {
      break;
    }
  }
}

void Pipsolar::loop() {
  if (this->state_ == STATE_IDLE) {
    this->empty_uart_buffer_();
    if (this->send_next_command_()) {
      return;
    }
    if (this->send_next_poll_()) {
      return;
    }
    return;
  }

  if (this->state_ == STATE_COMMAND_COMPLETE) {
    if (this->check_incoming_length_(4)) {
      if (this->check_incoming_crc_()) {
        if (this->read_buffer_[1] == 'A' && this->read_buffer_[2] == 'C' && this->read_buffer_[3] == 'K') {
          ESP_LOGD(TAG, "command successful");
        } else {
          ESP_LOGD(TAG, "command not successful");
        }
        this->command_queue_[this->command_queue_position_] = std::string("");
        this->command_queue_position_ = (command_queue_position_ + 1) % COMMAND_QUEUE_LENGTH;
        this->state_ = STATE_IDLE;
      } else {
        this->command_queue_[this->command_queue_position_] = std::string("");
        this->command_queue_position_ = (command_queue_position_ + 1) % COMMAND_QUEUE_LENGTH;
        this->state_ = STATE_IDLE;
      }
    } else {
      ESP_LOGD(TAG, "command %s response length not OK: with length %zu", this->command_queue_[this->command_queue_position_].c_str(), this->read_pos_);
      this->command_queue_[this->command_queue_position_] = std::string("");
      this->command_queue_position_ = (command_queue_position_ + 1) % COMMAND_QUEUE_LENGTH;
      this->state_ = STATE_IDLE;
    }
  }

  if (this->state_ == STATE_POLL_CHECKED) {
    ESP_LOGD(TAG, "poll %s decode", this->used_polling_commands_[this->last_polling_command_].command);
    this->handle_poll_response_(this->used_polling_commands_[this->last_polling_command_].identifier, (const char *) this->read_buffer_);
    this->state_ = STATE_IDLE;
    return;
  }

  if (this->state_ == STATE_POLL_COMPLETE) {
    if (this->check_incoming_crc_()) {
      if (this->read_buffer_[0] == '(' && this->read_buffer_[1] == 'N' && this->read_buffer_[2] == 'A' && this->read_buffer_[3] == 'K') {
        ESP_LOGD(TAG, "poll %s NACK", this->used_polling_commands_[this->last_polling_command_].command);
        this->handle_poll_error_(this->used_polling_commands_[this->last_polling_command_].identifier);
        this->state_ = STATE_IDLE;
        return;
      }
      this->used_polling_commands_[this->last_polling_command_].needs_update = false;
      this->state_ = STATE_POLL_CHECKED;
      return;
    } else {
      this->handle_poll_error_(this->used_polling_commands_[this->last_polling_command_].identifier);
      this->state_ = STATE_IDLE;
    }
  }

  if (this->state_ == STATE_COMMAND || this->state_ == STATE_POLL) {
    size_t avail = this->available();
    while (avail > 0) {
      uint8_t buf[64];
      size_t to_read = std::min(avail, sizeof(buf));
      if (!this->read_array(buf, to_read)) {
        break;
      }
      avail -= to_read;
      bool done = false;
      for (size_t i = 0; i < to_read; i++) {
        uint8_t byte = buf[i];
        if (this->read_pos_ >= PIPSOLAR_READ_BUFFER_LENGTH - 1) {
          this->read_pos_ = 0;
          this->empty_uart_buffer_();
          ESP_LOGW(TAG, "response data too long, discarding.");
          done = true;
          break;
        }
        this->read_buffer_[this->read_pos_] = byte;
        this->read_pos_++;
        if (byte == 0x0D) {
          this->read_buffer_[this->read_pos_] = 0;
          this->empty_uart_buffer_();
          if (this->state_ == STATE_POLL) {
            this->state_ = STATE_POLL_COMPLETE;
          }
          if (this->state_ == STATE_COMMAND) {
            this->state_ = STATE_COMMAND_COMPLETE;
          }
          done = true;
          break;
        }
      }
      if (done) {
        break;
      }
    }
  }

  if (this->state_ == STATE_COMMAND) {
    if (millis() - this->command_start_millis_ > Pipsolar::COMMAND_TIMEOUT) {
      const char *command = this->command_queue_[this->command_queue_position_].c_str();
      this->command_start_millis_ = millis();
      ESP_LOGD(TAG, "command %s timeout", command);
      this->command_queue_[this->command_queue_position_] = std::string("");
      this->command_queue_position_ = (command_queue_position_ + 1) % COMMAND_QUEUE_LENGTH;
      this->state_ = STATE_IDLE;
      return;
    }
  }

  if (this->state_ == STATE_POLL) {
    if (millis() - this->command_start_millis_ > Pipsolar::COMMAND_TIMEOUT) {
      ESP_LOGD(TAG, "poll %s timeout", this->used_polling_commands_[this->last_polling_command_].command);
      this->handle_poll_error_(this->used_polling_commands_[this->last_polling_command_].identifier);
      this->state_ = STATE_IDLE;
    }
  }
}

uint8_t Pipsolar::check_incoming_length_(uint8_t length) {
  if (this->read_pos_ >= 3 && this->read_pos_ - 3 == length) {
    return 1;
  }
  return 0;
}

uint8_t Pipsolar::check_incoming_crc_() {
  if (this->read_pos_ < 3) return 0;
  uint16_t crc16;
  crc16 = this->pipsolar_crc_(read_buffer_, read_pos_ - 3);
  if (((uint8_t) ((crc16) >> 8)) == read_buffer_[read_pos_ - 3] && ((uint8_t) ((crc16) & 0xff)) == read_buffer_[read_pos_ - 2]) {
    ESP_LOGD(TAG, "CRC OK");
    read_buffer_[read_pos_ - 1] = 0;
    read_buffer_[read_pos_ - 2] = 0;
    read_buffer_[read_pos_ - 3] = 0;
    return 1;
  }
  ESP_LOGD(TAG, "CRC NOK expected: %X %X but got: %X %X", ((uint8_t) ((crc16) >> 8)), ((uint8_t) ((crc16) & 0xff)), read_buffer_[read_pos_ - 3], read_buffer_[read_pos_ - 2]);
  return 0;
}

bool Pipsolar::send_next_command_() {
  uint16_t crc16;
  if (!this->command_queue_[this->command_queue_position_].empty()) {
    const char *command = this->command_queue_[this->command_queue_position_].c_str();
    uint8_t byte_command[16];
    size_t length = this->command_queue_[this->command_queue_position_].length();
    if (length > sizeof(byte_command)) {
      ESP_LOGE(TAG, "Command too long: %zu", length);
      this->command_queue_[this->command_queue_position_].clear();
      return false;
    }
    for (size_t i = 0; i < length; i++) {
      byte_command[i] = (uint8_t) this->command_queue_[this->command_queue_position_].at(i);
    }
    this->state_ = STATE_COMMAND;
    this->command_start_millis_ = millis();
    this->empty_uart_buffer_();
    this->read_pos_ = 0;
    crc16 = this->pipsolar_crc_(byte_command, length);
    this->write_str(command);
    this->write(((uint8_t) ((crc16) >> 8)));
    this->write(((uint8_t) ((crc16) & 0xff)));
    this->write(0x0D);
    ESP_LOGD(TAG, "Sending command from queue: %s with length %d", command, length);
    return true;
  }
  return false;
}

bool Pipsolar::send_next_poll_() {
  uint16_t crc16;
  for (uint8_t i = 0; i < POLLING_COMMANDS_MAX; i++) {
    this->last_polling_command_ = (this->last_polling_command_ + 1) % POLLING_COMMANDS_MAX;
    if (this->used_polling_commands_[this->last_polling_command_].length == 0) {
      continue;
    }
    if (!this->used_polling_commands_[this->last_polling_command_].needs_update) {
      continue;
    }
    this->state_ = STATE_POLL;
    this->command_start_millis_ = millis();
    this->empty_uart_buffer_();
    this->read_pos_ = 0;
    crc16 = this->pipsolar_crc_(this->used_polling_commands_[this->last_polling_command_].command, this->used_polling_commands_[this->last_polling_command_].length);
    this->write_array(this->used_polling_commands_[this->last_polling_command_].command, this->used_polling_commands_[this->last_polling_command_].length);
    this->write(((uint8_t) ((crc16) >> 8)));
    this->write(((uint8_t) ((crc16) & 0xff)));
    this->write(0x0D);
    ESP_LOGD(TAG, "Sending polling command: %s with length %d", this->used_polling_commands_[this->last_polling_command_].command, this->used_polling_commands_[this->last_polling_command_].length);
    return true;
  }
  return false;
}

void Pipsolar::queue_command(const std::string &command) {
  uint8_t next_position = command_queue_position_;
  for (uint8_t i = 0; i < COMMAND_QUEUE_LENGTH; i++) {
    uint8_t testposition = (next_position + i) % COMMAND_QUEUE_LENGTH;
    if (command_queue_[testposition].empty()) {
      command_queue_[testposition] = command;
      ESP_LOGD(TAG, "Command queued successfully: %s at position %d", command.c_str(), testposition);
      return;
    }
  }
  ESP_LOGD(TAG, "Command queue full dropping command: %s", command.c_str());
}

void Pipsolar::handle_poll_response_(ENUMPollingCommand polling_command, const char *message) {
  switch (polling_command) {
    case POLLING_QPIRI: handle_qpiri_(message); break;
    case POLLING_QPIGS: handle_qpigs_(message); break;
    case POLLING_QMOD: handle_qmod_(message); break;
    case POLLING_QFLAG: handle_qflag_(message); break;
    case POLLING_QPIWS: handle_qpiws_(message); break;
    case POLLING_QT: handle_qt_(message); break;
    case POLLING_QMN: handle_qmn_(message); break;
    case POLLING_QPIGS2: handle_qpigs2_(message); break;
    case POLLING_QBATCD: handle_qbatcd_(message); break;
    case POLLING_QPGS0: handle_qpgs0_(message); break;
    case POLLING_Q1: handle_q1_(message); break;
    case POLLING_QBMS: handle_qbms_(message); break;
    case POLLING_QET: handle_qet_(message); break;
    case POLLING_QLT: handle_qlt_(message); break;
    case POLLING_QMCHGCR: handle_qmchgcr_(message); break;
    case POLLING_QMUCHGCR: handle_qmuchgcr_(message); break;
    default: break;
  }
}

void Pipsolar::handle_poll_error_(ENUMPollingCommand polling_command) {
  this->handle_poll_response_(polling_command, "");
}

void Pipsolar::handle_qpiri_(const char *message) {
  if (this->last_qpiri_) {
    this->last_qpiri_->publish_state(message);
  }
  size_t pos = 0;
  this->skip_start_(message, &pos);

  this->read_float_sensor_(message, &pos, this->grid_rating_voltage_);
  this->read_float_sensor_(message, &pos, this->grid_rating_current_);
  this->read_float_sensor_(message, &pos, this->ac_output_rating_voltage_);
  this->read_float_sensor_(message, &pos, this->ac_output_rating_frequency_);
  this->read_float_sensor_(message, &pos, this->ac_output_rating_current_);
  this->read_int_sensor_(message, &pos, this->ac_output_rating_apparent_power_);
  this->read_int_sensor_(message, &pos, this->ac_output_rating_active_power_);
  this->read_float_sensor_(message, &pos, this->battery_rating_voltage_);

  std::string battery_recharge_voltage_str = this->read_field_(message, &pos);
  std::string battery_under_voltage_str = this->read_field_(message, &pos);
  std::string battery_bulk_voltage_str = this->read_field_(message, &pos);
  std::string battery_float_voltage_str = this->read_field_(message, &pos);
  std::string battery_type_str = this->read_field_(message, &pos);
  std::string current_max_ac_charging_current_str = this->read_field_(message, &pos);
  std::string current_max_charging_current_str2 = this->read_field_(message, &pos);

  esphome::optional<int> input_voltage_range = parse_number<int32_t>(this->read_field_(message, &pos));
  esphome::optional<int> output_source_priority = parse_number<int32_t>(this->read_field_(message, &pos));
  esphome::optional<int> charger_source_priority = parse_number<int32_t>(this->read_field_(message, &pos));
  this->read_int_sensor_(message, &pos, this->parallel_max_num_);
  this->read_int_sensor_(message, &pos, this->machine_type_);
  this->read_int_sensor_(message, &pos, this->topology_);
  this->read_int_sensor_(message, &pos, this->output_mode_);
  std::string battery_redischarge_voltage_str = this->read_field_(message, &pos);
  esphome::optional<int> pv_ok_condition_for_parallel = parse_number<int32_t>(this->read_field_(message, &pos));
  esphome::optional<int> pv_power_balance = parse_number<int32_t>(this->read_field_(message, &pos));
  std::string max_charging_time_at_cv_stage_str = this->read_field_(message, &pos);
  std::string operation_logic_str = this->read_field_(message, &pos);
  std::string max_discharging_current_str = this->read_field_(message, &pos);

  if (this->battery_recharge_voltage_) this->battery_recharge_voltage_->publish_state(parse_number<float>(battery_recharge_voltage_str).value_or(NAN));
  if (this->battery_under_voltage_) this->battery_under_voltage_->publish_state(parse_number<float>(battery_under_voltage_str).value_or(NAN));
  if (this->battery_bulk_voltage_) this->battery_bulk_voltage_->publish_state(parse_number<float>(battery_bulk_voltage_str).value_or(NAN));
  if (this->battery_float_voltage_) this->battery_float_voltage_->publish_state(parse_number<float>(battery_float_voltage_str).value_or(NAN));
  if (this->battery_type_) this->battery_type_->publish_state(parse_number<int32_t>(battery_type_str).value_or(NAN));
  if (this->current_max_ac_charging_current_) this->current_max_ac_charging_current_->publish_state(parse_number<int32_t>(current_max_ac_charging_current_str).value_or(NAN));
  if (this->current_max_charging_current_) this->current_max_charging_current_->publish_state(parse_number<int32_t>(current_max_charging_current_str2).value_or(NAN));

  if (this->input_voltage_range_) this->input_voltage_range_->publish_state(input_voltage_range.value_or(NAN));
  if (this->input_voltage_range_switch_ && input_voltage_range.has_value()) {
    this->input_voltage_range_switch_->publish_state(input_voltage_range.value() == 1);
  }

  if (this->output_source_priority_) this->output_source_priority_->publish_state(output_source_priority.value_or(NAN));
  if (this->charger_source_priority_) this->charger_source_priority_->publish_state(charger_source_priority.value_or(NAN));

  if (this->output_source_priority_select_) {
    this->output_source_priority_select_->map_and_publish(output_source_priority_str);
  }
  if (this->charger_source_priority_select_) {
    this->charger_source_priority_select_->map_and_publish(charger_source_priority_str);
  }
  if (this->current_max_ac_charging_current_select_) {
    this->current_max_ac_charging_current_select_->map_and_publish(current_max_ac_charging_current_str);
  }
  if (this->current_max_charging_current_select_) {
    this->current_max_charging_current_select_->map_and_publish(current_max_charging_current_str2);
  }
  if (this->battery_recharge_voltage_select_) {
    this->battery_recharge_voltage_select_->map_and_publish(battery_recharge_voltage_str);
  }
  if (this->battery_cutoff_voltage_select_) {
    this->battery_cutoff_voltage_select_->map_and_publish(battery_under_voltage_str);
  }
  if (this->battery_bulk_voltage_select_) {
    this->battery_bulk_voltage_select_->map_and_publish(battery_bulk_voltage_str);
  }
  if (this->battery_float_voltage_select_) {
    this->battery_float_voltage_select_->map_and_publish(battery_float_voltage_str);
  }
  if (this->battery_type_select_) {
    this->battery_type_select_->map_and_publish(battery_type_str);
  }
  if (this->battery_redischarge_voltage_select_) {
    this->battery_redischarge_voltage_select_->map_and_publish(battery_redischarge_voltage_str);
  }
  if (this->pv_ok_condition_for_parallel_) {
    this->pv_ok_condition_for_parallel_->publish_state(pv_ok_condition_for_parallel.value_or(NAN));
  }
  if (this->pv_ok_condition_for_parallel_switch_ && pv_ok_condition_for_parallel.has_value()) {
    this->pv_ok_condition_for_parallel_switch_->publish_state(pv_ok_condition_for_parallel.value() == 1);
  }
  if (this->pv_power_balance_) {
    this->pv_power_balance_->publish_state(pv_power_balance.value_or(NAN));
  }
  if (this->pv_power_balance_switch_ && pv_power_balance.has_value()) {
    this->pv_power_balance_switch_->publish_state(pv_power_balance.value() == 1);
  }
  if (this->max_discharging_current_) {
    this->max_discharging_current_->publish_state(parse_number<int32_t>(max_discharging_current_str).value_or(NAN));
  }
  if (this->max_discharging_current_select_) {
    this->max_discharging_current_select_->map_and_publish(max_discharging_current_str);
  }
  if (this->battery_max_bulk_charging_time_select_) {
    this->battery_max_bulk_charging_time_select_->map_and_publish(max_charging_time_at_cv_stage_str);
  }
  if (this->operation_logic_) {
    this->operation_logic_->publish_state(operation_logic_str);
  }
}

void Pipsolar::handle_qpigs_(const char *message) {
  if (this->last_qpigs_) {
    this->last_qpigs_->publish_state(message);
  }
  size_t pos = 0;
  this->skip_start_(message, &pos);
  this->read_float_sensor_(message, &pos, this->grid_voltage_);
  this->read_float_sensor_(message, &pos, this->grid_frequency_);
  this->read_float_sensor_(message, &pos, this->ac_output_voltage_);
  this->read_float_sensor_(message, &pos, this->ac_output_frequency_);
  this->read_int_sensor_(message, &pos, this->ac_output_apparent_power_);
  this->read_int_sensor_(message, &pos, this->ac_output_active_power_);
  this->read_int_sensor_(message, &pos, this->output_load_percent_);
  this->read_int_sensor_(message, &pos, this->bus_voltage_);
  this->read_float_sensor_(message, &pos, this->battery_voltage_);
  this->read_int_sensor_(message, &pos, this->battery_charging_current_);
  this->read_int_sensor_(message, &pos, this->battery_capacity_percent_);
  this->read_int_sensor_(message, &pos, this->inverter_heat_sink_temperature_);
  this->read_float_sensor_(message, &pos, this->pv1_input_current_);
  this->read_float_sensor_(message, &pos, this->pv1_input_voltage_);
  this->read_float_sensor_(message, &pos, this->battery_voltage_scc_);
  this->read_int_sensor_(message, &pos, this->battery_discharge_current_);
  
  std::string device_status_1 = this->read_field_(message, &pos);
  this->publish_binary_sensor_(this->get_bit_(device_status_1, 0), this->add_sbu_priority_version_);
  this->publish_binary_sensor_(this->get_bit_(device_status_1, 1), this->configuration_status_);
  this->publish_binary_sensor_(this->get_bit_(device_status_1, 2), this->scc_firmware_version_);
  this->publish_binary_sensor_(this->get_bit_(device_status_1, 3), this->load_status_);
  this->publish_binary_sensor_(this->get_bit_(device_status_1, 4), this->battery_voltage_to_steady_while_charging_);
  this->publish_binary_sensor_(this->get_bit_(device_status_1, 5), this->charging_status_);
  this->publish_binary_sensor_(this->get_bit_(device_status_1, 6), this->scc_charging_status_);
  this->publish_binary_sensor_(this->get_bit_(device_status_1, 7), this->ac_charging_status_);

  esphome::optional<int> battery_voltage_offset = parse_number<int32_t>(this->read_field_(message, &pos));
  if (this->battery_voltage_offset_for_fans_on_) {
    this->battery_voltage_offset_for_fans_on_->publish_state(battery_voltage_offset.value_or(NAN) / 10.0f);
  }

  this->read_int_sensor_(message, &pos, this->eeprom_version_);
  this->read_int_sensor_(message, &pos, this->pv1_charging_power_);

  std::string device_status_2 = this->read_field_(message, &pos);
  this->publish_binary_sensor_(this->get_bit_(device_status_2, 0), this->charging_to_floating_mode_);
  this->publish_binary_sensor_(this->get_bit_(device_status_2, 1), this->switch_on_);
  this->publish_binary_sensor_(this->get_bit_(device_status_2, 2), this->dustproof_installed_);

  auto solar_feed = parse_number<int32_t>(this->read_field_(message, &pos));
  this->publish_binary_sensor_(solar_feed.has_value() ? esphome::optional<bool>(solar_feed.value() != 0) : esphome::optional<bool>{}, this->solar_feed_to_grid_status_);

  this->read_int_sensor_(message, &pos, this->country_customized_regulation_);
  this->read_int_sensor_(message, &pos, this->solar_feed_to_grid_power_);
}

void Pipsolar::handle_qbatcd_(const char *message) {
  if (this->last_qbatcd_) {
    this->last_qbatcd_->publish_state(message);
  }
  if (this->charging_discharging_control_select_) {
    this->charging_discharging_control_select_->map_and_publish(std::string(message));
  }
}

void Pipsolar::handle_qbms_(const char *message) {
  if (this->last_qbms_) {
    this->last_qbms_->publish_state(message);
  }
  if (this->bms_values_select_) {
    this->bms_values_select_->map_and_publish(std::string(message));
  }
}

uint16_t Pipsolar::pipsolar_crc_(uint8_t *msg, uint8_t len) {
  uint16_t crc = crc16be(msg, len);
  uint8_t crc_low = crc & 0xff;
  uint8_t crc_high = crc >> 8;
  if (crc_low == 0x28 || crc_low == 0x0d || crc_low == 0x0a) crc_low++;
  if (crc_high == 0x28 || crc_high == 0x0d || crc_high == 0x0a) crc_high++;
  crc = (crc_high << 8) | crc_low;
  return crc;
}

}  // namespace pipsolar
}  // namespace esphome
