#include "signalk_number.h"
#include "esphome/core/log.h"

namespace esphome {
namespace signalk {
static const char *const TAG = "signalk";

void SignalkNumber::setup() { this->requires_update_ = true; }

void SignalkNumber::dump_config() {}

void SignalkNumber::update() {
  if (this->requires_update_) {
    this->requires_update_ = false;
  }
}

void SignalkNumber::control(float state) {
  this->publish_state(state);
  float converted_value = convert_to_base(state, this->unit_);
  this->parent_->publish_delta(this->path_, converted_value);
}

}  // namespace signalk
}  // namespace esphome
