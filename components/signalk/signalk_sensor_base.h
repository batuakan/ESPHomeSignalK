#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"

// #include "esphome/components/signalk/signalk.h"
class SignalK;

namespace esphome {
namespace signalk {

// template <typename T>
class SignalkSensorBase {
 public:
  virtual void on_delta_received(float value) {}
  virtual void on_delta_received(std::string value) {}

  // void set_parent(SignalK *parent) { parent_ = parent; }
  void set_path(std::string path) { path_ = path; }
  void set_period(int period) { period_ = period; }
  void set_format(std::string format) { format_ = format; }
  void set_policy(std::string policy) { policy_ = policy; }
  void set_min_period(int min_period) { min_period_ = min_period; }

  //  void get_parent() { return parent_; }
  std::string get_path() { return path_; }
  int get_period() { return period_; }
  std::string get_format() { return format_; }
  std::string get_policy() { return policy_; }
  int get_min_period() { return min_period_; }

 protected:
  // SignalK *parent_;
  std::string path_;
  int period_;
  std::string format_;
  std::string policy_;
  int min_period_;
};

}  // namespace signalk
}  // namespace esphome
