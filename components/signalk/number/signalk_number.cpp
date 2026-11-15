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

void SignalkNumber::enable_put() {
  this->parent_->add_put_handler(this->path_, [this](const std::string &, JsonVariantConst value) {
    double si;
    if (!put_value_number(value, si))
      return std::optional<PutReply>(PutReply{400, "Expected a number"});
    float v = convert_from_base(static_cast<float>(si), this->unit_);
    if (v < this->traits.get_min_value() || v > this->traits.get_max_value())
      return std::optional<PutReply>(PutReply{400, "Value out of range"});
    auto call = this->make_call();
    call.set_value(v);
    call.perform();
    return std::optional<PutReply>(PutReply{200, ""});
  });
}

void SignalkNumber::control(float state) {
  this->publish_state(state);
  float converted_value = convert_to_base(state, this->unit_);
  this->parent_->publish_delta(this->path_, converted_value);
}

}  // namespace signalk
}  // namespace esphome
