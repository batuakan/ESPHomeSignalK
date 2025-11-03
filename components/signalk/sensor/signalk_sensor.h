#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"

#include "esphome/components/signalk/signalk_subscriber.h"
#include "esphome/components/signalk/unit_conversions.h"

namespace esphome {
namespace signalk {

class SignalkSensor : public SignalKSubscriberTemplated<double>, public sensor::Sensor, public Component {
 public:
  void setup() override;
  void dump_config() override;

  void set_unit(Unit unit) { this->unit_ = unit; }
  void update() override;

 private:
  Unit unit_;
};

}  // namespace signalk
}  // namespace esphome
