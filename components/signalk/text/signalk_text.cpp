#include "signalk_text.h"
#include "esphome/core/log.h"

namespace esphome {
namespace signalk {
static const char *const TAG = "signalk";

void SignalkText::setup() 
{
  this->requires_update_ = true;
}

void SignalkText::dump_config() {}

void SignalkText::update() {
  if (this->requires_update_) {
    this->requires_update_ = false;
    
  }
}

void SignalkText::control(const std::string &state) {
  this->publish_state(state);
  this->parent_->publish_delta(this->path_, state);
  if (this->requires_metadata_update_) {
    this->requires_metadata_update_ = false;
    this->parent_->publish_meta_delta(this);
  }
}

}  // namespace signalk
}  // namespace esphome
