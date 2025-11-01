#pragma once

#include "esphome/core/component.h"
#include "esphome/components/switch/switch.h"

#include "esphome/components/signalk/signalk_sensor_base.h"

#include "../signalk.h"

namespace esphome {
namespace signalk {

class SignalkSwitch : public SignalkSensorBase, public switch_::Switch, public Component {
 public:
  void setup() override;
  void dump_config() override;
  void set_parent(SignalK *parent);
  void set_value(bool value) override;
  void update() override;
 protected:
  void write_state(bool state) override;
 private:
  bool value_;
  SignalK *parent_;
};

}  // namespace signalk
}  // namespace esphome
