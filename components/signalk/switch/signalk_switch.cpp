#include "signalk_switch.h"
#include "esphome/core/log.h"

namespace esphome {
namespace signalk {
static const char *const TAG = "signalk";

void SignalkSwitch::setup() 
{
  this->add_metadata("units", std::string("bool"));
  this->add_metadata("supportsPut", true);
  this->requires_update_ = true;
  
}

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
  this->parent_->publish_meta_delta(this);
}

}  // namespace signalk
}  // namespace esphome
