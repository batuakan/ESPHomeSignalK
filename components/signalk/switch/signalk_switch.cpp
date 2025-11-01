#include "signalk_switch.h"
#include "esphome/core/log.h"

namespace esphome {
namespace signalk {
static const char *const TAG = "signalk";

void SignalkSwitch::setup() {}

void SignalkSwitch::set_parent(SignalK *parent) {
  this->parent_ = parent;
}


void SignalkSwitch::set_value(bool value) {
  this->requires_update_ = true;
  this->value_ = value;
}

void SignalkSwitch::dump_config() {}

void SignalkSwitch::update() {
  if (this->requires_update_) {
    this->requires_update_ = false;
    this->publish_state(this->value_);
  }
}

void SignalkSwitch::write_state(bool state) {
  this->publish_state(state);
  this->parent_->publish_delta(this->path_, state);
}

}  // namespace signalk
}  // namespace esphome
