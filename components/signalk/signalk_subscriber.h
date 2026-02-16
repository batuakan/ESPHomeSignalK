#pragma once

#include "esphome/core/component.h"
#include <ArduinoJson.h>

#include "signalk_metadata.h"

namespace esphome {
namespace signalk {

class SignalK;

class SignalkSubscriber {
 public:
  virtual void update() = 0;
  virtual inline void set_value(JsonVariant &value) = 0;
  // virtual void set_value(bool value) {}
  // virtual void set_value(double value) {}
  // virtual void set_value(std::string value) {}

  void add_metadata(const std::string &key, const std::string &value);
  void add_metadata(const std::string &key, double value);
  void add_metadata(const std::string &key, bool value);
  void add_metadata(const std::string &key, const std::vector<std::string> &value);
  void add_metadata_from_json(const std::string &key, const std::string &json);
  void to_json(JsonVariant meta) const;

  void set_path(std::string path);
  void set_period(int period);
  void set_format(std::string format);
  void set_policy(std::string policy);
  void set_min_period(int min_period);

  std::string get_path();
  int get_period();
  std::string get_format();
  std::string get_policy();
  int get_min_period();

  void set_parent(SignalK *parent);

  virtual void disconnected();

 protected:
  std::string path_;
  int period_;
  std::string format_;
  std::string policy_;
  int min_period_;

  SignalK *parent_;

  bool requires_update_;
  bool requires_metadata_update_{true};
  SignalKMetadata metadata_;
};

template<typename T> class SignalKSubscriberTemplated : public SignalkSubscriber {
 public:
  inline void set_value(JsonVariant &value) override {
    this->requires_update_ = true;
    this->value_ = value.as<T>();
  }
  // void set_value(T &value);

 protected:
  T value_;
};

}  // namespace signalk
}  // namespace esphome
