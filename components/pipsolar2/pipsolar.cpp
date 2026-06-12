#include "pipsolar.h"
#include "esphome/core/log.h"
#include <cstring>

namespace esphome {
namespace pipsolar {

static const char *const TAG = "pipsolar";

void Pipsolar::setup() {
  this->empty_uart_buffer_();
}

void Pipsolar::loop() {
  if (this->state_ == STATE_IDLE) {
    if (!this->send_next_poll_()) {
      this->send_next_command_();
    }
  }

  while (this->available()) {
    if (this->read_pos_ >= sizeof(this->read_buffer_)) {
      this->read_pos_ = 0;
    }
    this->read_buffer_[this->read_pos_++] = this->read();
  }

  if (this->read_pos_ > 0) {
    if (this->check_incoming_length_(this->read_buffer_[0])) {
      if (this->check_incoming_crc_() == 0) {
        if (this->state_ == STATE_POLL) {
          this->state_ = STATE_POLL_CHECKED;
        } else if (this->state_ == STATE_COMMAND) {
          this->state_ = STATE_COMMAND_COMPLETE;
        }
      } else {
        ESP_LOGW(TAG, "CRC error");
        if (this->state_ == STATE_POLL) {
          this->handle_poll_error_((ENUMPollingCommand) this->last_polling_command_);
        }
        this->state_ = STATE_IDLE;
        this->read_pos_ = 0;
      }
    }
  }

  if (this->state_ == STATE_POLL_CHECKED) {
    this->state_ = STATE_POLL_DECODED;
    const char *message = reinterpret_cast<const char *>(this->read_buffer_ + 1);
    this->handle_poll_response_((ENUMPollingCommand) this->last_polling_command_, message);
    this->read_pos_ = 0;
    this->state_ = STATE_IDLE;
  } else if (this->state_ == STATE_COMMAND_COMPLETE) {
    this->read_pos_ = 0;
    this->state_ = STATE_IDLE;
  }

  if (this->command_start_millis_ != 0 && millis() - this->command_start_millis_ > COMMAND_TIMEOUT) {
    ESP_LOGW(TAG, "Command timeout");
    this->command_start_millis_ = 0;
    this->state_ = STATE_IDLE;
    this->read_pos_ = 0;
  }
}

void Pipsolar::update() {
  this->last_poll_ = millis();
}

void Pipsolar::dump_config() {
  ESP_LOGCONFIG(TAG, "PipSolar:");
  LOG_UART_DEVICE("  UART", this);
}

void Pipsolar::add_polling_command_(const char *command, ENUMPollingCommand polling_command) {
  for (size_t i = 0; i < sizeof(this->used_polling_commands_) / sizeof(PollingCommand); i++) {
    if (this->used_polling_commands_[i].length == 0) {
      this->used_polling_commands_[i].command = (uint8_t *) command;
      this->used_polling_commands_[i].length = strlen(command);
      this->used_polling_commands_[i].identifier = polling_command;
      this->used_polling_commands_[i].errors = 0;
      return;
    }
  }
  ESP_LOGE(TAG, "Too many polling commands");
}

bool Pipsolar::send_next_poll_() {
  for (size_t i = 0; i < sizeof(this->used_polling_commands_) / sizeof(PollingCommand); i++) {
    if (this->used_polling_commands_[i].length > 0) {
      this->last_polling_command_ = i;
      this->state_ = STATE_POLL;
      this->command_start_millis_ = millis();
      this->write_array(this->used_polling_commands_[i].command, this->used_polling_commands_[i].length);
      return true;
    }
  }
  return false;
}

bool Pipsolar::send_next_command_() {
  if (this->command_queue_position_ > 0) {
    this->state_ = STATE_COMMAND;
    this->command_start_millis_ = millis();
    std::string cmd = this->command_queue_[0];
    this->write_array((uint8_t *) cmd.c_str(), cmd.length());

    for (uint8_t i = 0; i < this->command_queue_position_ - 1; i++) {
      this->command_queue_[i] = this->command_queue_[i + 1];
    }
    this->command_queue_position_--;
    return true;
  }
  return false;
}

void Pipsolar::queue_command(const std::string &command) {
  this->queue_command_(command.c_str(), command.length());
}

void Pipsolar::queue_command_(const char *command, uint8_t length) {
  if (this->command_queue_position_ >= COMMAND_QUEUE_LENGTH) {
    ESP_LOGW(TAG, "Command queue full");
    return;
  }
  this->command_queue_[this->command_queue_position_] = std::string(command, length);
  this->command_queue_position_++;
}

void Pipsolar::empty_uart_buffer_() {
  while (this->available()) {
    this->read();
  }
  this->read_pos_ = 0;
}

uint16_t Pipsolar::pipsolar_crc_(uint8_t *msg, uint8_t len) {
  uint16_t crc = 0;
  for (uint8_t i = 0; i < len; i++) {
    crc += msg[i];
  }
  return crc;
}

uint8_t Pipsolar::check_incoming_length_(uint8_t length) {
  if (this->read_pos_ < length + 2) {
    return 0;
  }
  return 1;
}

uint8_t Pipsolar::check_incoming_crc_() {
  uint8_t len = this->read_buffer_[0];
  uint16_t received_crc = (this->read_buffer_[len + 1] << 8) | this->read_buffer_[len];
  uint16_t calculated_crc = this->pipsolar_crc_(this->read_buffer_, len);
  if (received_crc == calculated_crc) {
    return 0;
  }
  return 1;
}

void Pipsolar::handle_poll_error_(ENUMPollingCommand polling_command) {
  for (size_t i = 0; i < sizeof(this->used_polling_commands_) / sizeof(PollingCommand); i++) {
    if (this->used_polling_commands_[i].identifier == polling_command) {
      this->used_polling_commands_[i].errors++;
      if (this->used_polling_commands_[i].errors > 5) {
        ESP_LOGW(TAG, "Poll command %s too many errors, disabling", this->used_polling_commands_[i].command);
        this->used_polling_commands_[i].length = 0;
      }
      break;
    }
  }
}

void Pipsolar::handle_poll_response_(ENUMPollingCommand polling_command, const char *message) {
  ESP_LOGD(TAG, "poll %s decode", this->used_polling_commands_[this->last_polling_command_].command);
  size_t pos = 0;
  switch (polling_command) {
    case POLLING_QPIGS:
      this->handle_qpigs_(message);
      break;
    case POLLING_QPIRI:
      this->handle_qpiri_(message);
      break;
    case POLLING_QMOD:
      this->handle_qmod_(message);
      break;
    case POLLING_QFLAG:
      this->handle_qflag_(message);
      break;
    case POLLING_QPIWS:
      this->handle_qpiws_(message);
      break;
    case POLLING_QT:
      this->handle_qt_(message);
      break;
    case POLLING_QMN:
      this->handle_qmn_(message);
      break;
    case POLLING_HGEN:
      this->handle_hgen_(message);
      break;
    case POLLING_HPVB:
      this->handle_hpvb_(message);
      break;
    case POLLING_QPIGS2:
      this->handle_qpigs2_(message);
      break;
    case POLLING_QBATCD:
      this->handle_qbatcd_(message);
      break;
    case POLLING_QPGS0:
      this->handle_qpgs0_(message);
      break;
    case POLLING_Q1:
      this->handle_q1_(message);
      break;
    case POLLING_QBMS:
      this->handle_qbms_(message);
      break;
    case POLLING_QET:
      this->handle_qet_(message);
      break;
    case POLLING_QLT:
      this->handle_qlt_(message);
      break;
    case POLLING_QMCHGCR:
      this->handle_qmchgcr_(message);
      break;
    case POLLING_QMUCHGCR:
      this->handle_qmuchgcr_(message);
      break;
    default:
      ESP_LOGW(TAG, "Unknown poll command %d", polling_command);
      break;
  }
}

void Pipsolar::skip_start_(const char *message, size_t *pos) {
  while (message[*pos] != '(' && message[*pos] != 0) {
    (*pos)++;
  }
  if (message[*pos] == '(') {
    (*pos)++;
  }
}

std::string Pipsolar::read_field_(const char *message, size_t *pos) {
  std::string field;
  while (message[*pos] != ' ' && message[*pos] != ')' && message[*pos] != 0) {
    field += message[*pos];
    (*pos)++;
  }
  if (message[*pos] == ' ' || message[*pos] == ')') {
    (*pos)++;
  }
  return field;
}

void Pipsolar::read_float_sensor_(const char *message, size_t *pos, sensor::Sensor *sensor) {
  if (sensor == nullptr) return;
  std::string val = this->read_field_(message, pos);
  float f = std::stof(val);
  sensor->publish_state(f);
}
void Pipsolar::read_int_sensor_(const char *message, size_t *pos, sensor::Sensor *sensor) {
  if (sensor == nullptr) return;
  std::string val = this->read_field_(message, pos);
  int i = std::stoi(val);
  sensor->publish_state(i);
}

optional<bool> Pipsolar::get_bit_(const std::string &message, int index) {
  if ((size_t) index >= message.length()) return {};
  return message[index] == '1';
}

void Pipsolar::publish_binary_sensor_(optional<bool> state, binary_sensor::BinarySensor *sensor) {
  if (sensor && state.has_value()) {
    sensor->publish_state(*state);
  }
}

// 下面为各个指令解析函数，原有逻辑保留
void Pipsolar::handle_qpiri_(const char *message) {
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
  this->read_float_sensor_(message, &pos, this->battery_recharge_voltage_);
  this->read_float_sensor_(message, &pos, this->battery_under_voltage_);
  this->read_float_sensor_(message, &pos, this->battery_bulk_voltage_);
  this->read_float_sensor_(message, &pos, this->battery_float_voltage_);
  this->read_int_sensor_(message, &pos, this->battery_type_);
  this->read_int_sensor_(message, &pos, this->current_max_ac_charging_current_);
  this->read_int_sensor_(message, &pos, this->current_max_charging_current_);
  this->read_int_sensor_(message, &pos, this->input_voltage_range_);
  this->read_int_sensor_(message, &pos, this->output_source_priority_);
  this->read_int_sensor_(message, &pos, this->charger_source_priority_);
  this->read_int_sensor_(message, &pos, this->parallel_max_num_);
  this->read_int_sensor_(message, &pos, this->machine_type_);
  this->read_int_sensor_(message, &pos, this->topology_);
  this->read_int_sensor_(message, &pos, this->output_mode_);
  this->read_float_sensor_(message, &pos, this->battery_redischarge_voltage_);
  this->read_int_sensor_(message, &pos, this->pv_ok_condition_for_parallel_);
  this->read_int_sensor_(message, &pos, this->pv_power_balance_);

  if (this->last_qpiri_ != nullptr) {
    this->last_qpiri_->publish_state(std::string(message));
  }
}

void Pipsolar::handle_qpigs_(const char *message) {
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

  std::string bits = this->read_field_(message, &pos);
  this->publish_binary_sensor_(this->get_bit_(bits, 0), this->add_sbu_priority_version_);
  this->publish_binary_sensor_(this->get_bit_(bits, 1), this->configuration_status_);
  this->publish_binary_sensor_(this->get_bit_(bits, 2), this->scc_firmware_version_);
  this->publish_binary_sensor_(this->get_bit_(bits, 3), this->load_status_);
  this->publish_binary_sensor_(this->get_bit_(bits, 4), this->battery_voltage_to_steady_while_charging_);
  this->publish_binary_sensor_(this->get_bit_(bits, 5), this->charging_status_);
  this->publish_binary_sensor_(this->get_bit_(bits, 6), this->scc_charging_status_);
  this->publish_binary_sensor_(this->get_bit_(bits, 7), this->ac_charging_status_);

  this->read_int_sensor_(message, &pos, this->battery_voltage_offset_for_fans_on_);
  this->read_int_sensor_(message, &pos, this->eeprom_version_);
  this->read_int_sensor_(message, &pos, this->pv1_charging_power_);

  std::string bits2 = this->read_field_(message, &pos);
  this->publish_binary_sensor_(this->get_bit_(bits2, 0), this->charging_to_floating_mode_);
  this->publish_binary_sensor_(this->get_bit_(bits2, 1), this->switch_on_);
  this->publish_binary_sensor_(this->get_bit_(bits2, 2), this->dustproof_installed_);

  if (this->last_qpigs_ != nullptr) {
    this->last_qpigs_->publish_state(std::string(message));
  }
}

void Pipsolar::handle_qmod_(const char *message) {
  size_t pos = 0;
  this->skip_start_(message, &pos);
  std::string mode = this->read_field_(message, &pos);
  if (this->device_mode_ != nullptr) {
    this->device_mode_->publish_state(mode);
  }
  if (this->last_qmod_ != nullptr) {
    this->last_qmod_->publish_state(std::string(message));
  }
}

void Pipsolar::handle_qflag_(const char *message) {
  size_t pos = 0;
  this->skip_start_(message, &pos);
  std::string flags = this->read_field_(message, &pos);
  this->publish_binary_sensor_(this->get_bit_(flags, 0), this->silence_buzzer_open_buzzer_);
  this->publish_binary_sensor_(this->get_bit_(flags, 1), this->overload_bypass_function_);
  this->publish_binary_sensor_(this->get_bit_(flags, 2), this->lcd_escape_to_default_);
  this->publish_binary_sensor_(this->get_bit_(flags, 3), this->overload_restart_function_);
  this->publish_binary_sensor_(this->get_bit_(flags, 4), this->over_temperature_restart_function_);
  this->publish_binary_sensor_(this->get_bit_(flags, 5), this->backlight_on_);
  this->publish_binary_sensor_(this->get_bit_(flags, 6), this->alarm_on_when_primary_source_interrupt_);
  this->publish_binary_sensor_(this->get_bit_(flags, 7), this->fault_code_record_);
  this->publish_binary_sensor_(this->get_bit_(flags, 8), this->power_saving_);

  if (this->last_qflag_ != nullptr) {
    this->last_qflag_->publish_state(std::string(message));
  }
}

void Pipsolar::handle_qpiws_(const char *message) {
  size_t pos = 0;
  this->skip_start_(message, &pos);
  std::string warn_fault = this->read_field_(message, &pos);
  this->publish_binary_sensor_(this->get_bit_(warn_fault, 0), this->warning_power_loss_);
  this->publish_binary_sensor_(this->get_bit_(warn_fault, 1), this->fault_inverter_fault_);
  this->publish_binary_sensor_(this->get_bit_(warn_fault, 2), this->fault_bus_over_);
  this->publish_binary_sensor_(this->get_bit_(warn_fault, 3), this->fault_bus_under_);
  this->publish_binary_sensor_(this->get_bit_(warn_fault, 4), this->fault_bus_soft_fail_);
  this->publish_binary_sensor_(this->get_bit_(warn_fault, 5), this->warning_line_fail_);
  this->publish_binary_sensor_(this->get_bit_(warn_fault, 6), this->fault_opvshort_);
  this->publish_binary_sensor_(this->get_bit_(warn_fault, 7), this->fault_inverter_voltage_too_low_);
  this->publish_binary_sensor_(this->get_bit_(warn_fault, 8), this->fault_inverter_voltage_too_high_);
  this->publish_binary_sensor_(this->get_bit_(warn_fault, 9), this->warning_over_temperature_);
  this->publish_binary_sensor_(this->get_bit_(warn_fault, 10), this->warning_fan_lock_);
  this->publish_binary_sensor_(this->get_bit_(warn_fault, 11), this->warning_battery_voltage_high_);
  this->publish_binary_sensor_(this->get_bit_(warn_fault, 12), this->warning_battery_low_alarm_);
  this->publish_binary_sensor_(this->get_bit_(warn_fault, 13), this->warning_battery_under_shutdown_);
  this->publish_binary_sensor_(this->get_bit_(warn_fault, 14), this->warning_battery_derating_);
  this->publish_binary_sensor_(this->get_bit_(warn_fault, 15), this->warning_over_load_);
  this->publish_binary_sensor_(this->get_bit_(warn_fault, 16), this->warning_eeprom_failed_);
  this->publish_binary_sensor_(this->get_bit_(warn_fault, 17), this->fault_inverter_over_current_);
  this->publish_binary_sensor_(this->get_bit_(warn_fault, 18), this->fault_inverter_soft_failed_);
  this->publish_binary_sensor_(this->get_bit_(warn_fault, 19), this->fault_self_test_failed_);
  this->publish_binary_sensor_(this->get_bit_(warn_fault, 20), this->fault_op_dc_voltage_over_);
  this->publish_binary_sensor_(this->get_bit_(warn_fault, 21), this->fault_battery_open_);
  this->publish_binary_sensor_(this->get_bit_(warn_fault, 22), this->fault_current_sensor_failed_);
  this->publish_binary_sensor_(this->get_bit_(warn_fault, 23), this->fault_battery_short_);
  this->publish_binary_sensor_(this->get_bit_(warn_fault, 24), this->warning_power_limit_);
  this->publish_binary_sensor_(this->get_bit_(warn_fault, 25), this->warning_pv_voltage_high_);
  this->publish_binary_sensor_(this->get_bit_(warn_fault, 26), this->fault_mppt_overload_);
  this->publish_binary_sensor_(this->get_bit_(warn_fault, 27), this->warning_mppt_overload_);
  this->publish_binary_sensor_(this->get_bit_(warn_fault, 28), this->warning_battery_too_low_to_charge_);
  this->publish_binary_sensor_(this->get_bit_(warn_fault, 29), this->fault_dc_dc_over_current_);
  this->publish_binary_sensor_(this->get_bit_(warn_fault, 30), this->fault_code_);
  this->publish_binary_sensor_(this->get_bit_(warn_fault, 31), this->warnung_low_pv_energy_);
  this->publish_binary_sensor_(this->get_bit_(warn_fault, 32), this->warning_high_ac_input_during_bus_soft_start_);
  this->publish_binary_sensor_(this->get_bit_(warn_fault, 33), this->warning_battery_equalization_);

  bool has_warn = false, has_fault = false;
  for (char c : warn_fault) {
    if (c == '1') has_warn = true;
  }
  this->warnings_present_->publish_state(has_warn);
  this->faults_present_->publish_state(has_fault);

  if (this->last_qpiws_ != nullptr) {
    this->last_qpiws_->publish_state(std::string(message));
  }
}

void Pipsolar::handle_qt_(const char *message) {
  if (this->last_qt_ != nullptr) {
    this->last_qt_->publish_state(std::string(message));
  }
}

void Pipsolar::handle_qmn_(const char *message) {
  if (this->last_qmn_ != nullptr) {
    this->last_qmn_->publish_state(std::string(message));
  }
}

void Pipsolar::handle_hgen_(const char *message) {
  size_t pos = 0;
  this->skip_start_(message, &pos);
  this->read_int_sensor_(message, &pos, this->today_date_);
  this->read_int_sensor_(message, &pos, this->now_time_);
  this->read_int_sensor_(message, &pos, this->now_time_hour_);
  this->read_int_sensor_(message, &pos, this->now_time_min_);
  this->read_float_sensor_(message, &pos, this->daily_electricity_generation_);
  this->read_float_sensor_(message, &pos, this->monthly_electricity_generation_);
  this->read_float_sensor_(message, &pos, this->annual_electricity_generation_);
  this->read_float_sensor_(message, &pos, this->total_electricity_generation_);
}

void Pipsolar::handle_hpvb_(const char *message) {
  size_t pos = 0;
  this->skip_start_(message, &pos);
  this->read_float_sensor_(message, &pos, this->pv2_input_voltage_);
  this->read_float_sensor_(message, &pos, this->pv2_input_current_);
  this->read_float_sensor_(message, &pos, this->pv2_input_power_);
}

void Pipsolar::handle_qpigs2_(const char *message) {}
void Pipsolar::handle_qbatcd_(const char *message) {
  if (this->last_qbatcd_ != nullptr) {
    this->last_qbatcd_->publish_state(std::string(message));
  }
}
void Pipsolar::handle_qpgs0_(const char *message) {}
void Pipsolar::handle_q1_(const char *message) {}
void Pipsolar::handle_qbms_(const char *message) {
  if (this->last_qbms_ != nullptr) {
    this->last_qbms_->publish_state(std::string(message));
  }
  if (this->bms_values_select_ != nullptr) {
    this->bms_values_select_->map_and_publish(std::string(message));
  }
}
void Pipsolar::handle_qet_(const char *message) {}
void Pipsolar::handle_qlt_(const char *message) {}
void Pipsolar::handle_qmchgcr_(const char *message) {}
void Pipsolar::handle_qmuchgcr_(const char *message) {}

void Pipsolar::switch_command(const std::string &command) {
  this->queue_command(command);
}

}  // namespace pipsolar
}  // namespace esphome
