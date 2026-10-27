#pragma once
#include <atomic>
#include <deque>
#include <functional>
#include <map>
#include <mutex>
#include <set>
#include <vector>
#include <variant>
#include "esphome/core/component.h"
#include "esphome/core/automation.h"

// #include "esphome/components/sensor/sensor.h"
#include <ArduinoJson.h>
#include "signalk_subscriber.h"
#include "utils.h"
#include "unit_conversions.h"
#include "subscription_set.h"

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

/// Coarse access state for consumers that need to tell the user what to do.
enum class SignalKAccessState { NO_TOKEN = 0, PENDING_APPROVAL = 1, DENIED = 2, AUTHORIZED = 3 };

/// Called on the main loop with the value of a delta for a listened path.
using SignalKListener = std::function<void(const std::string &path, JsonVariantConst value)>;

class SignalK : public PollingComponent {
 public:
  void dump_config() override;
  void login();
  void on_message(const std::string &msg);
  void on_receive_delta(JsonArray &arr);
  void publish_delta(const std::string &path, const std::variant<double, std::string, bool> &value);
  /// Sends a meta delta for `path`; returns false when it could not be sent.
  bool publish_meta_delta(const std::string &path, JsonVariantConst meta);
  void put_request(const std::string &path, const std::variant<double, std::string, bool> &value);
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
  /// Register a YAML-declared subscriber (codegen, before setup()).
  void subscribe(SignalkSubscriber *sensor);
  void update() override;
  /// Drains the transport inbox and applies subscription changes; main loop only.
  void loop() override;

  /// Runtime listeners (main loop only). Returns an id for remove_listener().
  uint32_t add_listener(const std::string &path, SignalKListener callback);
  void remove_listener(uint32_t id);

  // Read-only state for other components (main loop only).
  const std::string &get_token() const { return token_; }
  const std::string &get_host() const { return host_; }
  unsigned short get_port() const { return port_; }
  std::string get_device_id() const { return get_uuid(); }
  bool is_stream_connected() const { return stream_connected_.load(); }
  SignalKAccessState access_state() const;

  virtual bool connect(const std::string &path) = 0;
  virtual bool discovery() = 0;
  virtual bool send(const std::string &msg) = 0;
  virtual bool is_connected() const = 0;

  virtual HttpResponse get(const std::string &path) = 0;
  virtual HttpResponse post(const std::string &path, const std::string &msg) = 0;

 protected:
  // Transport hand-off (safe from any thread): the WebSocket client task/thread only queues;
  // all parsing, callbacks and subscription changes happen in loop().
  void signal_connected();
  void signal_disconnected();
  void enqueue_message(std::string &&msg);

  void on_connected();
  void on_disconnected();
  void sync_subscriptions();
  void sync_metadata();
  bool sync_metadata_of(SignalkSubscriber *subscriber);

  std::string host_;
  unsigned short port_;
  std::string device_name_;
  std::string user_name_;
  std::string user_password_;
  std::string request_access_href_;
  std::string token_;

  std::vector<SignalkSubscriber *> sensors_;
  SubscriptionSet subscriptions_;
  uint32_t subscriptions_changed_at_{0};
  // Earliest time the next metadata sync may run (backoff after a failed server request).
  uint32_t metadata_next_attempt_at_{0};

  struct Listener {
    uint32_t id;
    std::string path;
    SignalKListener callback;
  };
  std::vector<Listener> listeners_;
  uint32_t next_listener_id_{1};

  std::mutex inbox_mutex_;
  std::deque<std::string> inbox_;
  size_t inbox_bytes_{0};
  uint32_t inbox_dropped_{0};
  std::atomic<bool> connected_event_{false};
  std::atomic<bool> disconnected_event_{false};
  std::atomic<bool> stream_connected_{false};

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

  // void play(Ts... x) override {
  void play(const Ts &...x) override {
    DeltaValue val;
    if (this->value_.has_value()) {
      val = this->value_.value(x...);  // YAML expression result
    } else {
      val = std::get<0>(std::tuple<Ts...>(x...));  // forward raw x
    }

    // Dispatch based on type
    if (std::holds_alternative<float>(val)) {
      this->parent_->publish_delta(this->path_, convert_to_base(std::get<float>(val), unit_));
    } else if (std::holds_alternative<bool>(val)) {
      this->parent_->publish_delta(this->path_, std::get<bool>(val));
    } else if (std::holds_alternative<std::string>(val)) {
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

template<typename... Ts> class PutRequestAction : public Action<Ts...> {
 public:
  PutRequestAction(SignalK *parent) : parent_(parent) {}
  // Let value_ be either float or string
  TEMPLATABLE_VALUE(DeltaValue, value)

  // void play(Ts... x) override {
  void play(const Ts &...x) override {
    DeltaValue val;
    if (this->value_.has_value()) {
      val = this->value_.value(x...);  // YAML expression result
    } else {
      val = std::get<0>(std::tuple<Ts...>(x...));  // forward raw x
    }

    // Dispatch based on type
    if (std::holds_alternative<float>(val)) {
      this->parent_->put_request(this->path_, convert_to_base(std::get<float>(val), unit_));
    } else if (std::holds_alternative<bool>(val)) {
      this->parent_->put_request(this->path_, std::get<bool>(val));
    } else if (std::holds_alternative<std::string>(val)) {
      this->parent_->put_request(this->path_, std::get<std::string>(val));
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
