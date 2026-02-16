#pragma once

#include "esphome/core/component.h"
#include "esphome/components/text_sensor/text_sensor.h"

#include "esphome/components/signalk/signalk_subscriber.h"

namespace esphome {
namespace signalk {

class SignalkTextSensor : public SignalKSubscriberTemplated<std::string>,
                          public text_sensor::TextSensor,
                          public Component {
 public:
  void setup() override;
  void dump_config() override;

  // void set_value(std::string value) override;
  void update() override;
};

}  // namespace signalk
}  // namespace esphome
