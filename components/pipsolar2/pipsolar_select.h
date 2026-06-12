#pragma once

#include <map>
#include <utility>

#include "esphome/components/select/select.h"
#include "esphome/core/component.h"
// 修正为你的组件目录 pipsolar2
#include "esphome/components/pipsolar2/pipsolar.h"

namespace esphome {
namespace pipsolar {

// 前向声明
class Pipsolar;

class PipsolarSelect : public Component, public select::Select {
 public:
  void set_parent(Pipsolar *const parent) { this->parent_ = parent; }
  void set_optimistic(bool optimistic) { this->optimistic_ = optimistic; }
  void add_mapping(const std::string &key, std::string value) { this->mapping_[key] = std::move(value); }
  void add_status_mapping(const std::string &key, std::string value) { this->status_mapping_[key] = std::move(value); }

  void dump_config() override;
  void control(const std::string &value) override;
  void map_and_publish(const std::string &value);

 protected:
  std::map<std::string, std::string> mapping_;
  std::map<std::string, std::string> status_mapping_;
  Pipsolar *parent_{nullptr};
  bool optimistic_{false};
};

}  // namespace pipsolar
}  // namespace esphome
