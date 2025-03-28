#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"

#include "esphome/components/signalk/signalk_sensor_base.h"

class SignalK;

namespace esphome {
namespace signalk {

class SignalkSensor : public SignalkSensorBase, public sensor::Sensor, public Component {
 public:
  void setup() override;
  void dump_config() override;

  void on_delta_received(float value) override;
};

}  // namespace signalk
}  // namespace esphome
