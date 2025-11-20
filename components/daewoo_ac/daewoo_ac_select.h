#pragma once

#include "esphome/core/component.h"
#include "esphome/components/select/select.h"
#include "daewoo_ac.h"

namespace esphome {
namespace daewoo_ac {

class DaewooACVaneSelect : public select::Select, public Component {
 public:
  void setup() override;
  void set_parent(DaewooAC *parent) { this->parent_ = parent; }
  // set_vane_type is kept for API compatibility but no longer used (only vertical vane select is supported)
  void set_vane_type(bool) {}

 protected:
  void control(const std::string &value) override;
  
  DaewooAC *parent_{nullptr};
};

}  // namespace daewoo_ac
}  // namespace esphome

