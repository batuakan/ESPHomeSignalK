#pragma once
#include <atomic>
#include <deque>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <type_traits>
#include <set>
#include <vector>
#include <variant>
#include "esphome/core/component.h"
#include "esphome/core/automation.h"

// #include "esphome/components/sensor/sensor.h"
#include "esphome/components/json/json_util.h"
#include "signalk_subscriber.h"
#include "utils.h"
#include "unit_conversions.h"
#include "subscription_set.h"
#include "server_discovery.h"
#include "access_request.h"
#include "put_handling.h"

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
/// Called on the main loop when a delta says that the metadata of a subscribed path changed.
using SignalKMetaListener = std::function<void(const std::string &path)>;

/// The answer to a PUT the server forwarded to the device.
struct PutReply {
  int status_code{200};
  std::string message;
};
/// Handles a PUT the server forwards for a path (main loop). Return the reply, or nullopt to answer later
/// with SignalK::put_reply() (within 60 s, after that the server has replied 504 itself).
using SignalKPutHandler = std::function<std::optional<PutReply>(const std::string &request_id, JsonVariantConst value)>;

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

  /// Handles PUTs for `path` on the device. The server forwards a PUT only to a client that has published
  /// the path with a `$source`, so publish it after every (re)connect (see add_on_connect_callback()).
  void add_put_handler(const std::string &path, SignalKPutHandler handler);
  bool has_put_handler(const std::string &path) const { return put_handlers_.count(path) > 0; }
  /// Answers a forwarded PUT. Only the first answer to a request is sent; late ones are dropped.
  void put_reply(const std::string &request_id, int status_code, const std::string &message = "");
  /// The id of the last forwarded PUT, for answering it from an automation.
  const std::string &last_put_request_id() const { return last_put_request_id_; }
  /// `$source` of the deltas the device publishes; empty sends none (the server then can't forward PUTs).
  void set_source(std::string source) { source_ = std::move(source); }
  const std::string &get_source() const { return source_; }
  /// Called on the main loop each time the stream (re)connects, after the subscriptions are sent.
  void add_on_connect_callback(std::function<void()> callback) { connect_callbacks_.push_back(std::move(callback)); }
  void send_access_request();
  void poll_access_request();
  /// The admin approved (token) or denied the access request.
  void access_request_decided(const AccessReplyInfo &reply);
  void validate_token();
  /// The token, kept in an ESPHome preference (on the host build too: ESPHOME_PREFDIR).
  void save_token();
  void load_token();
  /// The server the device is bound to (found with mDNS or chosen), kept next to the token.
  void save_server();
  bool load_server(ServerCandidate &out);
  /// The link of the access request waiting for approval, kept so that a restart asks it again.
  void save_access_href();
  bool load_access_href();

  void set_device_name(std::string device_name) { device_name_ = device_name; }
  void set_host(std::string host) { host_ = host; }
  void set_port(unsigned short port) { port_ = port; }
  void set_user_name(std::string user_name) { user_name_ = user_name; }
  void set_user_password(std::string user_password) { user_password_ = user_password; }
  void set_sensor_value(SignalkSubscriber *sensor, JsonVariant value);
  void setup() override;
  /// Register a YAML-declared subscriber (codegen, before setup()).
  void subscribe(SignalkSubscriber *sensor);
  /// Register metadata for a path that is only published (signalk.publish_delta); not subscribed to.
  void add_metadata_source(SignalkSubscriber *source) { meta_sources_.push_back(source); }
  void update() override;
  /// Drains the transport inbox and applies subscription changes; main loop only.
  void loop() override;

  /// Runtime listeners (main loop only). Returns an id for remove_listener().
  uint32_t add_listener(const std::string &path, SignalKListener callback);
  void remove_listener(uint32_t id);
  /// A hint that metadata changed, for reading it again (GET .../<path>/meta): a meta delta may hold only
  /// some keys, and the server's minPeriod throttle may drop it, so read again now and then as well.
  void add_meta_listener(SignalKMetaListener callback) { meta_listeners_.push_back(std::move(callback)); }

  // Read-only state for other components (main loop only).
  const std::string &get_token() const { return token_; }
  const std::string &get_host() const { return host_; }
  unsigned short get_port() const { return port_; }
  std::string get_device_id() const { return get_uuid(); }
  bool is_stream_connected() const { return stream_connected_.load(); }
  /// Deltas (messages with `updates`) processed since boot, and messages dropped because the inbox was full.
  uint32_t deltas_received() const { return deltas_received_; }
  uint32_t messages_dropped() const { return messages_dropped_; }
  /// sk_millis() when the last delta was processed; 0 before the first.
  uint32_t last_delta_ms() const { return last_delta_ms_; }
  /// Paths subscribed to (YAML sensors and runtime listeners).
  size_t subscribed_paths() const { return subscriptions_.size(); }
  SignalKAccessState access_state() const;

  // Choosing the server (main loop only). With `host` in the configuration there is nothing to choose and
  // these do nothing.
  /// True when the server is set in the configuration rather than found with mDNS.
  bool server_configured() const { return host_configured_; }
  /// The server in use (empty until one is found), as found with mDNS.
  const ServerCandidate &current_server() const { return discovered_server_; }
  /// Starts a new mDNS search; servers_found() holds its result once scanning() is false.
  void scan_servers() { scan_requested_ = true; }
  bool scanning() const { return scan_requested_; }
  /// The usable servers the last finished search answered with.
  const std::vector<ServerCandidate> &servers_found() const { return servers_found_; }
  /// Uses `server` from now on, also after a restart. Another server than the current one drops the token,
  /// so the device asks that server for access.
  void choose_server(const ServerCandidate &server);
  /// Drops the token and the bound server, and looks for a server again.
  void forget_server();

  virtual bool connect(const std::string &path) = 0;
  /// Closes the stream; the transport reports the disconnect as usual.
  virtual void disconnect() = 0;
  /// One non-blocking step of the mDNS search for a server; called again until it returns DONE.
  /// The first call starts a search, DONE hands over everything found (possibly nothing) and the next
  /// call starts a new search.
  virtual DiscoveryPoll poll_discovery(std::vector<ServerCandidate> &found) = 0;
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
  /// True when the server to talk to is known. With no `host` configured it is looked up with mDNS.
  bool resolve_server();
  /// One step of a search asked for with scan_servers() while the server is known.
  void poll_scan();
  /// Commits to `server`: it is used and saved, so that only it is accepted after a restart.
  void bind_server(const ServerCandidate &server);
  /// Uses `server`, which matches the bound server only by its vessel id, without saving it: it is bound
  /// once it accepts the token (confirm_server), and given up if it refuses it (refuse_tried_server).
  void try_server(const ServerCandidate &server);
  /// The server being tried accepted the token or gave one: bind it.
  void confirm_server();
  /// The server being tried refused the token: skip it and look for the bound server again, token kept.
  void refuse_tried_server();
  /// Stops trying a server: the bound one is used (and looked for) again.
  void back_to_bound_server();
  /// An HTTP request got no response: a found server may have moved, look for it again.
  void server_unreachable();
  void sync_metadata();
  bool sync_metadata_of(SignalkSubscriber *subscriber);
  void handle_forwarded_put(const ForwardedPut &put);

  std::map<std::string, SignalKPutHandler> put_handlers_;
  PendingPuts pending_puts_;
  std::string last_put_request_id_;
  std::string source_;
  std::vector<std::function<void()>> connect_callbacks_;

  std::string host_;
  // The host was found with mDNS (not configured), so it is looked up again after a disconnect.
  bool host_discovered_{false};
  ServerCandidate discovered_server_;  // what was found, so that only this server is accepted again
  bool trying_server_{false};          // discovered_server_ is being tried (try_server), not bound
  ServerCandidate bound_server_;       // the bound server while another is tried
  std::vector<ServerCandidate> rejected_servers_;
  bool network_was_up_{false};
  bool rediscovering_{false};
  uint32_t rediscovery_started_at_{0};
  uint32_t discovery_polls_without_result_{0};
  bool host_configured_{false};
  bool scan_requested_{false};
  std::vector<ServerCandidate> servers_found_;
  unsigned short port_;
  std::string device_name_;
  std::string user_name_;
  std::string user_password_;
  std::string request_access_href_;
  // Earliest time the next access request may be sent.
  uint32_t next_access_request_at_{0};
  // Replies in a row to the pending request's link that weren't understood.
  uint32_t access_poll_unreadable_{0};
  std::string token_;

  std::vector<SignalkSubscriber *> sensors_;
  std::vector<SignalkSubscriber *> meta_sources_;
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
  std::vector<SignalKMetaListener> meta_listeners_;
  uint32_t next_listener_id_{1};

  std::mutex inbox_mutex_;
  std::deque<std::string> inbox_;
  size_t inbox_bytes_{0};
  uint32_t inbox_dropped_{0};
  uint32_t deltas_received_{0};
  uint32_t messages_dropped_{0};
  uint32_t last_delta_ms_{0};
  std::atomic<bool> connected_event_{false};
  std::atomic<bool> disconnected_event_{false};
  std::atomic<bool> stream_connected_{false};

  SignalKPreferedAccessMethod prefered_access_method_{SignalKPreferedAccessMethod::REQUEST_ACCESS};
  SignalKRequestAccessState request_access_state_{SignalKRequestAccessState::UNKNOWN};
  SignalKLoginState login_state_{SignalKLoginState::UNKNOWN};
};

/// Holds the metadata of a path the device publishes with signalk.publish_delta. It never receives values.
class SignalkPublishedMeta : public SignalkSubscriber {
 public:
  void update() override {}
  void set_value(JsonVariant &value) override {}
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
    } else if constexpr (sizeof...(Ts) > 0) {
      val = std::get<0>(std::tuple<Ts...>(x...));  // forward raw x
    } else {
      // No trigger argument to forward (e.g. on_boot): without a value there is nothing to send.
      ESP_LOGW("signalk", "No value to send to %s: set 'value:' or use this action where the trigger has a value",
               this->path_.c_str());
      return;
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
    } else if constexpr (sizeof...(Ts) > 0) {
      val = std::get<0>(std::tuple<Ts...>(x...));  // forward raw x
    } else {
      // No trigger argument to forward (e.g. on_boot): without a value there is nothing to send.
      ESP_LOGW("signalk", "No value to send to %s: set 'value:' or use this action where the trigger has a value",
               this->path_.c_str());
      return;
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

/// signalk.forget_server: drops the bound server and its token (SignalK::forget_server). Does nothing when
/// `host` is set.
template<typename... Ts> class ForgetServerAction : public Action<Ts...> {
 public:
  explicit ForgetServerAction(SignalK *parent) : parent_(parent) {}
  void play(const Ts &...x) override { this->parent_->forget_server(); }

 protected:
  SignalK *parent_;
};

/// on_put: runs an automation for each PUT the server forwards for a path, with the value as `x` and the
/// request id as `request_id`. T is float (converted from SI with `unit`), bool or std::string.
template<typename T> class PutTrigger : public Trigger<T, std::string> {
 public:
  PutTrigger(SignalK *parent, const std::string &path, bool auto_reply, Unit unit) {
    parent->add_put_handler(
        path,
        [this, auto_reply, unit](const std::string &request_id, JsonVariantConst value) -> std::optional<PutReply> {
          T x{};
          if (!convert(value, unit, x))
            return PutReply{400, "Invalid value"};
          this->trigger(x, request_id);
          // An automation that answered (signalk.put_reply) already took the request.
          if (auto_reply)
            return PutReply{200, ""};
          return std::nullopt;
        });
  }

 protected:
  static bool convert(JsonVariantConst value, Unit unit, T &out) {
    if constexpr (std::is_same_v<T, float>) {
      double n;
      if (!put_value_number(value, n))
        return false;
      out = convert_from_base(static_cast<float>(n), unit);
      return true;
    } else if constexpr (std::is_same_v<T, bool>) {
      return put_value_bool(value, out);
    } else {
      out = put_value_text(value);
      return true;
    }
  }
};

/// on_connect: runs each time the stream (re)connects, e.g. to publish the paths the device handles PUTs for.
class ConnectTrigger : public Trigger<> {
 public:
  explicit ConnectTrigger(SignalK *parent) {
    parent->add_on_connect_callback([this]() { this->trigger(); });
  }
};

/// signalk.put_reply: answers a forwarded PUT (for on_put with `reply: manual`, or to refuse one early).
template<typename... Ts> class PutReplyAction : public Action<Ts...> {
 public:
  explicit PutReplyAction(SignalK *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(std::string, request_id)
  TEMPLATABLE_VALUE(int, status_code)
  TEMPLATABLE_VALUE(std::string, message)

  void play(const Ts &...x) override {
    std::string id =
        this->request_id_.has_value() ? this->request_id_.value(x...) : this->parent_->last_put_request_id();
    int code = this->status_code_.has_value() ? this->status_code_.value(x...) : 200;
    std::string message = this->message_.has_value() ? this->message_.value(x...) : std::string();
    this->parent_->put_reply(id, code, message);
  }

 protected:
  SignalK *parent_;
};

}  // namespace signalk
}  // namespace esphome
