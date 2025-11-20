#pragma once

#include "esphome/core/component.h"
#include "esphome/components/switch/switch.h"
#include "daewoo_ac.h"

namespace esphome {
namespace daewoo_ac {

class DaewooACDisplaySwitch : public switch_::Switch, public Component {
 public:
  void setup() override;
  void loop() override;
  void set_parent(DaewooAC *parent) { this->parent_ = parent; }

 protected:
  void write_state(bool state) override;

  DaewooAC *parent_{nullptr};
  bool last_reported_state_{false};
};

}  // namespace daewoo_ac
}  // namespace esphome


