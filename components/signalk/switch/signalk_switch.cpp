#include "signalk_switch.h"
#include "esphome/core/log.h"

namespace esphome {
namespace signalk {
static const char *const TAG = "signalk";

void SignalkSwitch::setup() { this->requires_update_ = true; }

void SignalkSwitch::dump_config() {}

void SignalkSwitch::update() {
  if (this->requires_update_) {
    this->requires_update_ = false;
    if (this->value_)
      this->turn_on();
    else
      this->turn_off();
  }
}

void SignalkSwitch::write_state(bool state) {
  this->publish_state(state);
  this->parent_->publish_delta(this->path_, state);
}

}  // namespace signalk
}  // namespace esphome
