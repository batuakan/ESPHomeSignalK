#pragma once

#include "esphome/core/component.h"
#include "esphome/components/text_sensor/text_sensor.h"

#include "esphome/components/signalk/signalk_sensor_base.h"
class SignalK;

namespace esphome {
namespace signalk {

class SignalkTextSensor : public SignalkSensorBase, public text_sensor::TextSensor, public Component {
 public:
  void setup() override;
  void dump_config() override;

  void on_delta_received(std::string value) override;

 protected:
};

}  // namespace signalk
}  // namespace esphome
