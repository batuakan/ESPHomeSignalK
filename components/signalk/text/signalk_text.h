#pragma once

#include "esphome/core/component.h"
#include "../signalk.h"
#include "esphome/components/text/text.h"

#include "esphome/components/signalk/signalk_subscriber.h"
#include "esphome/components/signalk/unit_conversions.h"



namespace esphome {
namespace signalk {

class SignalkText : public SignalKSubscriberTemplated<std::string>, public text::Text, public Component {
 public:
  void setup() override;
  void dump_config() override;
  void update() override;
 protected:
  void control(const std::string &value) override;

};

}  // namespace signalk
}  // namespace esphome
