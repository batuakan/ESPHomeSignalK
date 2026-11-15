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

void SignalkSwitch::enable_put() {
  this->parent_->add_put_handler(this->path_, [this](const std::string &, JsonVariantConst value) {
    bool on;
    if (!put_value_bool(value, on))
      return std::optional<PutReply>(PutReply{400, "Expected true or false"});
    if (on)
      this->turn_on();
    else
      this->turn_off();
    return std::optional<PutReply>(PutReply{200, ""});
  });
}

void SignalkSwitch::write_state(bool state) {
  this->publish_state(state);
  this->parent_->publish_delta(this->path_, state);
}

}  // namespace signalk
}  // namespace esphome
