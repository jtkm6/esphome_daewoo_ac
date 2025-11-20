#pragma once

#include "esphome/core/component.h"
#include "esphome/components/switch/switch.h"
#include "daewoo_ac.h"

namespace esphome {
namespace daewoo_ac {

class DaewooACUVLightSwitch : public switch_::Switch, public Component {
 public:
  void setup() override;
  void set_parent(DaewooAC *parent) { this->parent_ = parent; }

 protected:
  void write_state(bool state) override;

  DaewooAC *parent_{nullptr};
};

}  // namespace daewoo_ac
}  // namespace esphome



