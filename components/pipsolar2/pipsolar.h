#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"

namespace esphome {
namespace pipsolar {

// 1. 前向声明 PipsolarSelect，解决循环依赖问题
class PipsolarSelect;

// 2. 定义枚举和结构体
enum ENUMPollingCommand {
  POLLING_QPIRI = 0,
  POLLING_QPIGS = 1,
  POLLING_QMOD = 2,
  POLLING_QFLAG = 3,
  POLLING_QPIWS = 4,
  POLLING_QT = 5,
  POLLING_QMN = 6,
  POLLING_HGEN = 7,
  POLLING_HPVB = 8,
  POLLING_QPIGS2 = 9,
  POLLING_QBATCD = 10,
  POLLING_QPGS0 = 11,
  POLLING_Q1 = 12,
  POLLING_QBMS = 13,
  POLLING_QET = 14,
  POLLING_QLT = 15,
  POLLING_QMCHGCR = 16,
  POLLING_QMUCHGCR = 17,
};

struct PollingCommand {
  uint8_t *command;
  uint8_t length = 0;
  uint8_t errors;
  ENUMPollingCommand identifier;
  bool needs_update = false;
};

#define POLLING_COMMANDS_MAX 18

// 3. 宏定义
#define PIPSOLAR_VALUED_ENTITY_(type, name, polling_command, value_type) \
 protected: \
  value_type value_##name##_; \
  PIPSOLAR_ENTITY_(type, name, polling_command)

#define PIPSOLAR_ENTITY_(type, name, polling_command) \
 protected: \
  type *name##_{}; /* NOLINT */ \
\
 public: \
  void set_##name(type *name) { /* NOLINT */ \
    this->name##_ = name; \
    this->add_polling_command_(#polling_command, POLLING_##polling_command); \
  }

#define PIPSOLAR_SENSOR(name, polling_command, value_type) \
  PIPSOLAR_VALUED_ENTITY_(sensor::Sensor, name, polling_command, value_type)
#define PIPSOLAR_SWITCH(name, polling_command) PIPSOLAR_ENTITY_(switch_::Switch, name, polling_command)
#define PIPSOLAR_BINARY_SENSOR(name, polling_command, value_type) \
  PIPSOLAR_VALUED_ENTITY_(binary_sensor::BinarySensor, name, polling_command, value_type)
#define PIPSOLAR_VALUED_TEXT_SENSOR(name, polling_command, value_type) \
  PIPSOLAR_VALUED_ENTITY_(text_sensor::TextSensor, name, polling_command, value_type)
#define PIPSOLAR_TEXT_SENSOR(name, polling_command) PIPSOLAR_ENTITY_(text_sensor::TextSensor, name, polling_command)

// 4. 主类定义
class Pipsolar : public uart::UARTDevice, public PollingComponent {
  // ... 你的其他成员变量（传感器、开关等）...

  // Select 相关成员变量（现在 PipsolarSelect 已经前向声明，编译器能识别了）
  PipsolarSelect *output_source_priority_select_{};
  PipsolarSelect *charger_source_priority_select_{};
  PipsolarSelect *battery_recharge_voltage_select_{};
  PipsolarSelect *battery_cutoff_voltage_select_{};
  PipsolarSelect *battery_bulk_voltage_select_{};
  PipsolarSelect *battery_float_voltage_select_{};
  PipsolarSelect *battery_type_select_{};
  PipsolarSelect *current_max_ac_charging_current_select_{};
  PipsolarSelect *current_max_charging_current_select_{};
  PipsolarSelect *battery_redischarge_voltage_select_{};
  PipsolarSelect *max_discharging_current_select_{};
  PipsolarSelect *battery_max_bulk_charging_time_select_{};
  PipsolarSelect *charging_discharging_control_select_{};
  PipsolarSelect *bms_values_select_{};

  // ... 你的成员函数声明 ...

 protected:
  // ... 你的其他成员变量 ...
  PollingCommand used_polling_commands_[15];
};

}  // namespace pipsolar
}  // namespace esphome

// 5. 最后再包含 pipsolar_select.h，此时 Pipsolar 类已经完全定义，PipsolarSelect 可以正常使用
#include "pipsolar_select.h"
