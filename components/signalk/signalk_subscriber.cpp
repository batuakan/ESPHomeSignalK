#include "signalk.h"
#include "signalk_subscriber.h"

namespace esphome {
namespace signalk {

// -------- Virtual hooks (default no-ops) --------
void SignalkSubscriber::update() {
  // Default empty implementation
}

void SignalkSubscriber::set_value(JsonVariant &value) {
  // Default empty implementation
  // Derived subscribers override this if they need value input
}

// -------- Metadata forwarding --------

void SignalkSubscriber::add_metadata(const std::string &key, const std::string &value) {
  this->metadata_.set(key, value);
}

void SignalkSubscriber::add_metadata(const std::string &key, double value) { this->metadata_.set(key, value); }

void SignalkSubscriber::add_metadata(const std::string &key, bool value) { this->metadata_.set(key, value); }

void SignalkSubscriber::add_metadata(const std::string &key, const std::vector<std::string> &value) {
  this->metadata_.set_array(key, value);
}

void SignalkSubscriber::add_metadata_from_json(const std::string &key, const std::string &json) {
  this->metadata_.set_json(key, json);
}

void SignalkSubscriber::to_json(JsonVariant meta) const { this->metadata_.to_json(meta); }

// -------- Configuration setters --------

void SignalkSubscriber::set_path(std::string path) { this->path_ = std::move(path); }

void SignalkSubscriber::set_period(int period) { this->period_ = period; }

void SignalkSubscriber::set_format(std::string format) { this->format_ = std::move(format); }

void SignalkSubscriber::set_policy(std::string policy) { this->policy_ = std::move(policy); }

void SignalkSubscriber::set_min_period(int min_period) { this->min_period_ = min_period; }

// -------- Accessors --------

std::string SignalkSubscriber::get_path() { return this->path_; }

int SignalkSubscriber::get_period() { return this->period_; }

std::string SignalkSubscriber::get_format() { return this->format_; }

std::string SignalkSubscriber::get_policy() { return this->policy_; }

int SignalkSubscriber::get_min_period() { return this->min_period_; }

// -------- Parent pointer --------

void SignalkSubscriber::set_parent(SignalK *parent) { this->parent_ = parent; }

// -------- Disconnected --------

void SignalkSubscriber::disconnected() {
  this->requires_metadata_update_ = true;
  this->requires_update_ = true;
}

}  // namespace signalk
}  // namespace esphome
