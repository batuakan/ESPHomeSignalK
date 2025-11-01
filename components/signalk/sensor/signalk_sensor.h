#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"

#include "esphome/components/signalk/signalk_sensor_base.h"
#include "esphome/components/signalk/unit_conversions.h"

namespace esphome {
namespace signalk {

class SignalkSensor : public SignalkSensorBase, public sensor::Sensor, public Component {
 public:
  void setup() override;
  void dump_config() override;

  void set_value(double value) override;
  void set_unit(Unit unit) { this->unit_ = unit; }
  void update() override;

 private:
  double value_;
  Unit unit_;
};

}  // namespace signalk
}  // namespace esphome
