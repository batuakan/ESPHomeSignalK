#include "signalk_text.h"
#include "esphome/core/log.h"

namespace esphome {
namespace signalk {
static const char *const TAG = "signalk";

void SignalkText::setup() { this->requires_update_ = true; }

void SignalkText::dump_config() {}

void SignalkText::update() {
  if (this->requires_update_) {
    this->requires_update_ = false;
  }
}

void SignalkText::enable_put() {
  this->parent_->add_put_handler(this->path_, [this](const std::string &, JsonVariantConst value) {
    auto call = this->make_call();
    call.set_value(put_value_text(value));
    call.perform();
    return std::optional<PutReply>(PutReply{200, ""});
  });
}

void SignalkText::control(const std::string &state) {
  this->publish_state(state);
  this->parent_->publish_delta(this->path_, state);
}

}  // namespace signalk
}  // namespace esphome
