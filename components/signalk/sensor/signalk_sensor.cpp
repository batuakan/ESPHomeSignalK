#include "signalk_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace signalk {
static const char *const TAG = "signalk";

void SignalkSensor::setup() {}

void SignalkSensor::on_delta_received(float value) { this->publish_state(value); }

void SignalkSensor::dump_config() {}
}  // namespace signalk
}  // namespace esphome
