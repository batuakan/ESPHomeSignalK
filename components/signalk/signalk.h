#pragma once
#include <map>
#include <variant>
#include "esphome/core/component.h"
#include "esphome/core/automation.h"

// #include "esphome/components/sensor/sensor.h"
#include <ArduinoJson.h>
#include "signalk_subscriber.h"
#include "utils.h"
#include "unit_conversions.h"

namespace esphome {
namespace signalk {


enum class SignalKPreferedAccessMethod { LOGIN = 0, REQUEST_ACCESS = 1 };

enum class SignalKLoginState { UNKNOWN = 0, COMPLETED = 1, DENIED = 2 };

enum class SignalKTokenValidationStatus { UNKNOWN = 0, COMPLETED = 1 };

enum class SignalKRequestAccessState { UNKNOWN = 0, HASTOKEN = 1, PENDING = 2, COMPLETED = 3, DENIED = 4 };

struct HttpResponse {
  int status_code;
  std::string body;
};

class SignalK : public PollingComponent {
 public:
  void dump_config() override;
  void login();
  void on_connected();
  void on_disconnected();
  void on_message(const std::string &msg);
  void on_receive_delta(JsonArray &arr);
  void publish_delta(const std::string &path, const std::variant<double, std::string, bool> &value);
  void publish_meta_delta(SignalkSubscriber *subscriber);
  void send_access_request();
  void poll_access_request();
  void validate_token();
  virtual void save_token();
  virtual void load_token();

  void set_device_name(std::string device_name) { device_name_ = device_name; }
  void set_host(std::string host) { host_ = host; }
  void set_port(unsigned short port) { port_ = port; }
  void set_user_name(std::string user_name) { user_name_ = user_name; }
  void set_user_password(std::string user_password) { user_password_ = user_password; }
  void set_sensor_value(SignalkSubscriber *sensor, JsonVariant value);
  void setup() override;
  void subscribe(SignalkSubscriber *sensor) {
    sensors_.insert(std::pair<std::string, SignalkSubscriber *>(sensor->get_path(), sensor));
  }
  void update() override;

  virtual bool connect(const std::string &path) = 0;
  virtual bool discovery() = 0;
  virtual bool send(const std::string &msg) = 0;
  // virtual void update() = 0;
  virtual bool is_connected() const = 0;

  virtual HttpResponse get(const std::string &path) = 0;
  virtual HttpResponse post(const std::string &path, const std::string &msg) = 0;

 protected:
  std::string host_;
  unsigned short port_;
  std::string device_name_;
  std::string user_name_;
  std::string user_password_;
  std::string request_access_href_;
  std::string token_;

  std::map<std::string, SignalkSubscriber *> sensors_;

  SignalKPreferedAccessMethod prefered_access_method_{SignalKPreferedAccessMethod::REQUEST_ACCESS};
  SignalKRequestAccessState request_access_state_{SignalKRequestAccessState::UNKNOWN};
  SignalKLoginState login_state_{SignalKLoginState::UNKNOWN};
};

using DeltaValue = std::variant<float, std::string, bool>;

template<typename... Ts> class PublishDeltaAction : public Action<Ts...> {
 public:
  PublishDeltaAction(SignalK *parent) : parent_(parent) {}
  // Let value_ be either float or string
  TEMPLATABLE_VALUE(DeltaValue, value)

  void play(Ts... x) override {
    DeltaValue val;
    if (this->value_.has_value()) {
      val = this->value_.value(x...);  // YAML expression result
    } else {
      val = std::get<0>(std::tuple<Ts...>(x...));  // forward raw x
    }

    // Dispatch based on type
    if (std::holds_alternative<float>(val)) {
      this->parent_->publish_delta(this->path_, convert_to_base(std::get<float>(val), unit_));
    } else if (std::holds_alternative<bool>(val)){
      this->parent_->publish_delta(this->path_, std::get<bool>(val));
    } 
    else if (std::holds_alternative<std::string>(val)) {
      this->parent_->publish_delta(this->path_, std::get<std::string>(val));
    }
  }

  void set_path(std::string path) { path_ = path; }
  void set_unit(Unit unit) { this->unit_ = unit; }

 protected:
  SignalK *parent_;
  std::string path_;
  Unit unit_;
};

}  // namespace signalk
}  // namespace esphome
