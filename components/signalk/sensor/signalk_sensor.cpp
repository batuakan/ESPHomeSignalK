#include "signalk_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace signalk {
static const char *const TAG = "signalk";

void SignalkSensor::setup() {}

void SignalkSensor::set_value(double value) {
  this->requires_update_ = true;
  this->value_ = value;
}

void SignalkSensor::dump_config() {}

void SignalkSensor::update() {
  if (this->requires_update_) {
    this->requires_update_ = false;
    float converted_value = convert_from_base(this->value_, this->unit_);
    this->publish_state(converted_value);
  }
}

}  // namespace signalk
}  // namespace esphome
