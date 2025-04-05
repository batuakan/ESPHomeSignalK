#include "signalk_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace signalk {
static const char *const TAG = "signalk";

void SignalkSensor::setup() {}

void SignalkSensor::set_value(float value) { 
    this->requires_update_ = true;
    this->value_ = value; 
}

void SignalkSensor::dump_config() {}

void SignalkSensor::update() {
    if (this->requires_update_) {
        this->requires_update_ = false;
        this->publish_state(this->value_);
    }
}


}  // namespace signalk
}  // namespace esphome
