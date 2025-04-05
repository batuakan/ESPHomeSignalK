#include "signalk_text_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace signalk {
static const char *const TAG = "signalk";

void SignalkTextSensor::setup() {}

void SignalkTextSensor::set_value(std::string value) { 
    this->requires_update_ = true;
    this->value_ = value; 
}

void SignalkTextSensor::dump_config() {}

void SignalkTextSensor::update() {
    if (this->requires_update_) {
        this->requires_update_ = false;
        this->publish_state(this->value_);
    }
}

}  // namespace signalk
}  // namespace esphome
