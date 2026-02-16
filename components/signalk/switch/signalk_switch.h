#pragma once

#include "esphome/core/component.h"
#include "../signalk.h"
#include "esphome/components/switch/switch.h"

#include "esphome/components/signalk/signalk_subscriber.h"

namespace esphome {
namespace signalk {

class SignalkSwitch : public SignalKSubscriberTemplated<bool>, public switch_::Switch, public Component {
 public:
  void setup() override;
  void dump_config() override;

  void update() override;

 protected:
  void write_state(bool state) override;
};

}  // namespace signalk
}  // namespace esphome
