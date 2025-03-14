#include "signalk_text_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace signalk {
static const char *const TAG = "signalk";

void SignalkTextSensor::setup() {}

void SignalkTextSensor::on_delta_received(std::string value) { this->publish_state(value); }

void SignalkTextSensor::dump_config() {}

}  // namespace signalk
}  // namespace esphome
