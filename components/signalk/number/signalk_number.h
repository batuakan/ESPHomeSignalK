#pragma once

#include "esphome/core/component.h"
#include "../signalk.h"
#include "esphome/components/number/number.h"

#include "esphome/components/signalk/signalk_subscriber.h"
#include "esphome/components/signalk/unit_conversions.h"



namespace esphome {
namespace signalk {

class SignalkNumber : public SignalKSubscriberTemplated<float>, public number::Number, public Component {
 public:
  void setup() override;
  void dump_config() override;
  void set_unit(Unit unit) { this->unit_ = unit; }
  void update() override;
 protected:
  // void write_state(float state) override;
  void control(float value) override;

  Unit unit_;
};

}  // namespace signalk
}  // namespace esphome
