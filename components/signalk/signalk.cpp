

#include "esphome/core/preferences.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include <algorithm>
#include <regex>
// #include "esphome/components/wifi/wifi_component.h"

#include "signalk.h"
#include "utils.h"

namespace esphome {
namespace signalk {

static const char *const TAG = "signalk";

// Bounds of the transport inbox; oldest messages are dropped beyond these.
static const size_t INBOX_MAX_MESSAGES = 64;
static const size_t INBOX_MAX_BYTES = 128 * 1024;
// Subscription changes are batched for this long before being sent.
static const uint32_t SUBSCRIPTION_DEBOUNCE_MS = 200;
// Wait this long before asking the server for metadata again after a failed request.
static const uint32_t METADATA_RETRY_MS = 10000;

/// Logs only the first characters of a secret.
static std::string redact(const std::string &secret) {
  return secret.empty() ? std::string("<none>") : secret.substr(0, 4) + "...";
}

void SignalK::subscribe(SignalkSubscriber *sensor) {
  sensors_.push_back(sensor);
  SubscriptionRow row;
  row.path = sensor->get_path();
  row.period = sensor->get_period();
  row.format = sensor->get_format();
  row.policy = sensor->get_policy();
  row.min_period = sensor->get_min_period();
  subscriptions_.add(row);
}

uint32_t SignalK::add_listener(const std::string &path, SignalKListener callback) {
  uint32_t id = next_listener_id_++;
  listeners_.push_back(Listener{id, path, std::move(callback)});
  SubscriptionRow row;
  row.path = path;
  subscriptions_.add(row);
  subscriptions_changed_at_ = sk_millis();
  return id;
}

void SignalK::remove_listener(uint32_t id) {
  for (auto it = listeners_.begin(); it != listeners_.end(); ++it) {
    if (it->id == id) {
      subscriptions_.remove(it->path);
      listeners_.erase(it);
      subscriptions_changed_at_ = sk_millis();
      return;
    }
  }
}

SignalKAccessState SignalK::access_state() const {
  if (prefered_access_method_ == SignalKPreferedAccessMethod::LOGIN) {
    if (login_state_ == SignalKLoginState::DENIED)
      return SignalKAccessState::DENIED;
    return token_.empty() ? SignalKAccessState::NO_TOKEN : SignalKAccessState::AUTHORIZED;
  }
  switch (request_access_state_) {
    case SignalKRequestAccessState::PENDING:
      return SignalKAccessState::PENDING_APPROVAL;
    case SignalKRequestAccessState::DENIED:
      return SignalKAccessState::DENIED;
    case SignalKRequestAccessState::COMPLETED:
      return token_.empty() ? SignalKAccessState::NO_TOKEN : SignalKAccessState::AUTHORIZED;
    default:
      return SignalKAccessState::NO_TOKEN;
  }
}

// -------- Transport hand-off (any thread) --------

void SignalK::signal_connected() {
  stream_connected_ = true;
  connected_event_ = true;
}

void SignalK::signal_disconnected() {
  stream_connected_ = false;
  disconnected_event_ = true;
}

void SignalK::enqueue_message(std::string &&msg) {
  std::lock_guard<std::mutex> lock(inbox_mutex_);
  inbox_bytes_ += msg.size();
  inbox_.push_back(std::move(msg));
  while (!inbox_.empty() && (inbox_.size() > INBOX_MAX_MESSAGES || inbox_bytes_ > INBOX_MAX_BYTES)) {
    inbox_bytes_ -= inbox_.front().size();
    inbox_.pop_front();
    inbox_dropped_++;
  }
}

// -------- Main loop --------

void SignalK::loop() {
  if (disconnected_event_.exchange(false)) {
    this->on_disconnected();
  }
  if (connected_event_.exchange(false)) {
    this->on_connected();
  }

  std::deque<std::string> batch;
  uint32_t dropped;
  {
    std::lock_guard<std::mutex> lock(inbox_mutex_);
    batch.swap(inbox_);
    inbox_bytes_ = 0;
    dropped = inbox_dropped_;
    inbox_dropped_ = 0;
  }
  if (dropped > 0) {
    ESP_LOGW(TAG, "Dropped %u queued messages (inbox full)", (unsigned) dropped);
  }
  for (auto &msg : batch) {
    this->on_message(msg);
  }
  // Publish new values of YAML-declared sensors right away (they only flag changes).
  for (auto *sensor : sensors_) {
    sensor->update();
  }

  if (stream_connected_ && subscriptions_.pending() != SubscriptionSet::Action::NONE &&
      sk_millis() - subscriptions_changed_at_ >= SUBSCRIPTION_DEBOUNCE_MS) {
    this->sync_subscriptions();
  }
  if (stream_connected_ && sk_millis() >= metadata_next_attempt_at_) {
    this->sync_metadata();
  }
}

static std::string subscribe_message(const std::vector<SubscriptionRow> &rows) {
  JsonDocument doc;
  doc["context"] = "vessels.self";
  JsonArray subscribe = doc["subscribe"].to<JsonArray>();
  for (const auto &r : rows) {
    JsonObject subscription = subscribe.add<JsonObject>();
    subscription["path"] = r.path;
    subscription["period"] = r.period;
    subscription["format"] = r.format;
    subscription["policy"] = r.policy;
    subscription["minPeriod"] = r.min_period;
  }
  std::string output;
  serializeJson(doc, output);
  return output;
}

void SignalK::sync_subscriptions() {
  switch (subscriptions_.pending()) {
    case SubscriptionSet::Action::NONE:
      return;
    case SubscriptionSet::Action::SUBSCRIBE_ADDED: {
      auto rows = subscriptions_.added();
      ESP_LOGD(TAG, "Subscribing to %u more paths", (unsigned) rows.size());
      if (!send(subscribe_message(rows)))
        return;
      break;
    }
    case SubscriptionSet::Action::RESUBSCRIBE_ALL: {
      // signalk-server only supports unsubscribing from everything; a per-path unsubscribe
      // closes the socket. So drop all and subscribe the full set again.
      auto rows = subscriptions_.all();
      ESP_LOGD(TAG, "Resubscribing to %u paths", (unsigned) rows.size());
      if (!send(R"({"context":"*","unsubscribe":[{"path":"*"}]})"))
        return;
      if (!rows.empty() && !send(subscribe_message(rows)))
        return;
      break;
    }
  }
  subscriptions_.mark_synced();
}

void SignalK::setup() {
  ESP_LOGD(TAG, "Setup");

  if (!this->user_name_.empty() && !this->user_password_.empty()) {
    prefered_access_method_ = SignalKPreferedAccessMethod::LOGIN;
    ESP_LOGD(TAG, "Using LOGIN as prefered access method");
    return;
  }

  load_token();
  ESP_LOGD(TAG, "Using REQUEST_ACCESS as prefered access method");
  prefered_access_method_ = SignalKPreferedAccessMethod::REQUEST_ACCESS;

  if (!token_.empty()) {
    request_access_state_ = SignalKRequestAccessState::HASTOKEN;
  }
}

void SignalK::update() {
  if (prefered_access_method_ == SignalKPreferedAccessMethod::REQUEST_ACCESS) {
    if (this->request_access_state_ == SignalKRequestAccessState::HASTOKEN)
      this->validate_token();
    else if (this->request_access_state_ == SignalKRequestAccessState::UNKNOWN)
      this->send_access_request();
    else if (this->request_access_state_ == SignalKRequestAccessState::PENDING)
      this->poll_access_request();
    else if (this->request_access_state_ == SignalKRequestAccessState::COMPLETED && !is_connected())
      this->connect("/signalk/v1/stream?subscribe=none");
  } else {
    if (prefered_access_method_ == SignalKPreferedAccessMethod::LOGIN && login_state_ == SignalKLoginState::UNKNOWN)
      this->login();
    else if (this->login_state_ == SignalKLoginState::COMPLETED && !is_connected())
      this->connect("/signalk/v1/stream?subscribe=none");
  }
}

void SignalK::dump_config() {}

void SignalK::on_connected() {
  // A fresh connection has no subscriptions: send the full set now (no debounce).
  ESP_LOGI(TAG, "Connected to SignalK stream");
  subscriptions_.mark_disconnected();
  metadata_next_attempt_at_ = 0;
  this->sync_subscriptions();
}

void SignalK::on_disconnected() {
  ESP_LOGW(TAG, "Disconnected from SignalK server");
  login_state_ = SignalKLoginState::UNKNOWN;
  if (!token_.empty())
    request_access_state_ = SignalKRequestAccessState::HASTOKEN;
  else
    request_access_state_ = SignalKRequestAccessState::UNKNOWN;
  for (auto it = sensors_.begin(); it != sensors_.end(); ++it) {
    // it->second->disconnected();
    (*it)->disconnected();
  }
}

void SignalK::login() {
  if (login_state_ == SignalKLoginState::COMPLETED) {
    return;
  }
  // Throttle login requests to once every 2 seconds
  static uint32_t last_login_attempt = 0;
  uint32_t now = sk_millis();
  if (now - last_login_attempt < 2000) {
    return;
  }
  last_login_attempt = now;

  JsonDocument doc;
  doc["username"] = this->user_name_;
  doc["password"] = this->user_password_;
  std::string msg;
  serializeJson(doc, msg);
  ESP_LOGD(TAG, "Trying to login as %s", this->user_name_.c_str());
  auto response = post("/signalk/v1/auth/login", msg);
  ESP_LOGD(TAG, "Login request response: HTTP %d", response.status_code);  // body holds the token

  JsonDocument resp_doc;
  DeserializationError error = deserializeJson(resp_doc, response.body);
  if (error) {
    ESP_LOGE(TAG, "Failed to parse access request response: %s", error.c_str());
    return;
  }

  token_ = resp_doc["token"] | "";
  if (!token_.empty()) {
    save_token();
    login_state_ = SignalKLoginState::COMPLETED;
    ESP_LOGI(TAG, "Login request successful");
  } else if (resp_doc["result"] == 401) {
    login_state_ = SignalKLoginState::DENIED;
    ESP_LOGE(TAG, "Login request denied");
  }
}

void SignalK::send_access_request() {
  if (!token_.empty()) {
    request_access_state_ = SignalKRequestAccessState::HASTOKEN;
    return;
  }
  // Throttle login requests to once every 2 seconds
  static uint32_t last_request_attempt = 0;
  uint32_t now = sk_millis();
  if (now - last_request_attempt < 2000) {
    return;
  }
  last_request_attempt = now;
  JsonDocument doc;
  doc["clientId"] = get_uuid();
  doc["description"] = "ESPHome device: " + App.get_name();
  // Displays PUT (switches, autopilot) and publish deltas, so ask for read/write up front.
  doc["permissions"] = "readwrite";
  std::string msg;
  serializeJson(doc, msg);

  ESP_LOGD(TAG, "Trying to request access: %s", msg.c_str());
  auto response = post("/signalk/v1/access/requests", msg);
  ESP_LOGD(TAG, "Access request response: %s", response.body.c_str());

  JsonDocument resp_doc;
  DeserializationError error = deserializeJson(resp_doc, response.body);
  if (error) {
    ESP_LOGE(TAG, "Failed to parse access request response: %s", error.c_str());
    return;
  } else if (resp_doc["state"] == "PENDING") {
    request_access_state_ = SignalKRequestAccessState::PENDING;
    request_access_href_ = resp_doc["href"] | "";
    ESP_LOGW(TAG, "Access request is pending approval");
  } else if (resp_doc["state"] == "COMPLETED") {
    if (resp_doc["statusCode"] == 400) {
      request_access_href_ = resp_doc["href"] | "";
      if (!request_access_href_.empty()) {
        request_access_state_ = SignalKRequestAccessState::PENDING;
        return;
      }
      request_access_state_ = SignalKRequestAccessState::DENIED;
      ESP_LOGE(TAG, "Access request denied");
      return;
    }
  }
}

void SignalK::poll_access_request() {
  // Throttle down polling to once per second
  static uint32_t last_request_access_attempt = 0;
  uint32_t now = sk_millis();
  if (now - last_request_access_attempt < 1000) {
    return;
  }
  last_request_access_attempt = now;

  auto response = get(request_access_href_);
  ESP_LOGD(TAG, "Access request status response: HTTP %d", response.status_code);  // body may hold the token

  JsonDocument resp_doc;
  DeserializationError error = deserializeJson(resp_doc, response.body);
  if (error) {
    ESP_LOGE(TAG, "Failed to parse access request status response: %s", error.c_str());
    return;
  }
  std::string state = resp_doc["state"] | "UNKNOWN";
  if (state == "COMPLETED") {
    if (resp_doc["accessRequest"]["permission"] == "DENIED") {
      request_access_state_ = SignalKRequestAccessState::DENIED;
      ESP_LOGE(TAG, "Access request denied");
      return;
    }

    if (resp_doc["accessRequest"]["permission"] == "APPROVED") {
      request_access_state_ = SignalKRequestAccessState::COMPLETED;
      token_ = resp_doc["accessRequest"]["token"] | "";
      if (!token_.empty()) {
        save_token();
      }
      ESP_LOGI(TAG, "Access request approved, token %s", redact(token_).c_str());
    }
  }
}

void SignalK::validate_token() {
  static uint32_t last_validation_attempt = 0;
  uint32_t now = sk_millis();
  if (now - last_validation_attempt < 2000) {
    return;
  }
  last_validation_attempt = now;
  ESP_LOGD(TAG, "Validating token");
  auto response = get("/signalk/v1/stream");
  ESP_LOGD(TAG, "Token validation response: HTTP %d", response.status_code);
  if (response.status_code == 426) {
    request_access_state_ = SignalKRequestAccessState::COMPLETED;
    ESP_LOGI(TAG, "Token validation successful");
  } else if (response.status_code == 401) {
    token_.clear();
    save_token();
    request_access_state_ = SignalKRequestAccessState::UNKNOWN;
    ESP_LOGE(TAG, "Token validation failed: HTTP %d", response.status_code);
  } else {
    ESP_LOGW(TAG, "Unexpected response during token validation: HTTP %d", response.status_code);
  }
}

void SignalK::save_token() {
  constexpr size_t TOKEN_MAX_LENGTH = 256;
  char token_buffer[TOKEN_MAX_LENGTH] = {0};
  uint32_t hash = fnv1_hash("signalk" + this->host_);
  auto pref = global_preferences->make_preference<char[TOKEN_MAX_LENGTH]>(10000, true);
  memcpy(token_buffer, token_.c_str(), std::min(token_.size(), sizeof(token_buffer) - 1));
  if (pref.save(&token_buffer)) {
    ESP_LOGI(TAG, "Saved token to preferences: %s", redact(token_).c_str());
  } else {
    ESP_LOGE(TAG, "Failed to save token");
  }
}

void SignalK::load_token() {
  constexpr size_t TOKEN_MAX_LENGTH = 256;
  char token_buffer[TOKEN_MAX_LENGTH] = {0};
  uint32_t hash = fnv1_hash("signalk" + this->host_);
  auto pref = global_preferences->make_preference<char[TOKEN_MAX_LENGTH]>(10000, true);
  if (pref.load(&token_buffer)) {
    token_ = std::string(token_buffer);
    if (!token_.empty()) {
      request_access_state_ = SignalKRequestAccessState::HASTOKEN;
      ESP_LOGI(TAG, "Loaded token: %s", redact(token_).c_str());
      return;
    }
  } else {
    ESP_LOGW(TAG, "No token stored in flash");
  }
}

void SignalK::on_message(const std::string &msg) {
  ESP_LOGV(TAG, "Received message: %s", msg.c_str());
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, msg);
  if (error) {
    ESP_LOGW(TAG, "Ignoring unparsable message (%u bytes): %s", (unsigned) msg.size(), error.c_str());
    return;
  }
  // A delta may carry several updates (e.g. one per source); process all of them.
  for (JsonObject update : doc["updates"].as<JsonArray>()) {
    JsonArray arr = update["values"].as<JsonArray>();
    if (!arr.isNull())
      this->on_receive_delta(arr);
  }
  JsonArray put_arr = doc["put"].as<JsonArray>();
  if (!put_arr.isNull())
    this->on_receive_delta(put_arr);
}

void SignalK::on_receive_delta(JsonArray &arr) {
  // JsonArray arr = doc["updates"][0]["values"].as<JsonArray>();
  for (JsonVariant delta : arr) {
    if (!delta["path"].is<std::string>()) {
      continue;
    }
    auto path = delta["path"].as<std::string>();
    // bool found = false;
    // auto sensor = sensors_[path];

    // if (sensor == NULL)
    // {
    for (auto it = sensors_.begin(); it != sensors_.end(); ++it) {
      if ((*it)->get_path() == path) {
        set_sensor_value(*it, delta["value"]);
      }
    }
    // Exact key match was not found, lets try regexish matching
    for (auto it = sensors_.begin(); it != sensors_.end(); ++it) {
      if (match_path(path, (*it)->get_path())) {
        set_sensor_value(*it, delta["value"]);
      }
    }
    // Runtime listeners. Iterate over a snapshot of ids: a callback may add or remove listeners.
    std::vector<uint32_t> ids;
    for (const auto &l : listeners_) {
      if (match_path(path, l.path))
        ids.push_back(l.id);
    }
    for (uint32_t id : ids) {
      for (const auto &l : listeners_) {
        if (l.id == id) {
          SignalKListener callback = l.callback;  // the callback may remove itself
          callback(path, delta["value"].as<JsonVariantConst>());
          break;
        }
      }
    }
    // for (auto it = sensors_.begin(); it != sensors_.end(); ++it) {
    //   if (std::regex_match(path, std::regex((*it)->get_path()))) {
    //     set_sensor_value((*it), delta["value"]);
    //   }
    // }

    // if (sensor != NULL)
    // {
    //   set_sensor_value(sensor, delta["value"]);
    // }
  }
}

void SignalK::set_sensor_value(SignalkSubscriber *sensor, JsonVariant value) {
  if (sensor == NULL) {
    return;
  }
  sensor->set_value(value);
}

void SignalK::put_request(const std::string &path, const std::variant<double, std::string, bool> &value) {
  if (!is_connected()) {
    return;
  }
  JsonDocument doc;
  doc["context"] = "vessels.self";
  doc["requestId"] = generate_requestid();
  JsonObject put = doc["put"].to<JsonObject>();
  put["path"] = path;

  if (std::holds_alternative<double>(value)) {
    put["value"] = std::get<double>(value);
  } else if (std::holds_alternative<bool>(value)) {
    put["value"] = std::get<bool>(value);
  } else {
    put["value"] = std::get<std::string>(value);
  }

  std::string output;
  serializeJson(doc, output);
  ESP_LOGD(TAG, "Put request: %s", output.c_str());
  send(output);
}

void SignalK::publish_delta(const std::string &path, const std::variant<double, std::string, bool> &value) {
  if (!is_connected()) {
    return;
  }
  JsonDocument doc;
  JsonArray updates = doc["updates"].to<JsonArray>();

  JsonObject update = updates.add<JsonObject>();

  // JsonObject source = update["source"].to<JsonObject>();
  // source["src"] = get_uuid();

  // auto timestamp = iso_timestamp();
  // update["timestamp"] = timestamp;

  JsonArray values = update["values"].to<JsonArray>();
  JsonObject val = values.add<JsonObject>();
  val["path"] = path;
  if (std::holds_alternative<double>(value)) {
    val["value"] = std::get<double>(value);
  } else if (std::holds_alternative<bool>(value)) {
    val["value"] = std::get<bool>(value);
  } else {
    val["value"] = std::get<std::string>(value);
  }

  std::string output;
  serializeJson(doc, output);
  ESP_LOGD(TAG, "Publishing delta: %s", output.c_str());
  send(output);
}

bool SignalK::publish_meta_delta(const std::string &path, JsonVariantConst meta) {
  if (!is_connected()) {
    return false;
  }
  JsonDocument doc;
  JsonArray updates = doc["updates"].to<JsonArray>();

  JsonObject update = updates.add<JsonObject>();

  JsonArray metas = update["meta"].to<JsonArray>();
  JsonObject entry = metas.add<JsonObject>();
  entry["path"] = path;
  entry["value"] = meta;

  std::string output;
  serializeJson(doc, output);
  ESP_LOGD(TAG, "Publishing meta delta: %s", output.c_str());
  return send(output);
}

// Metadata on the server can be newer than what is compiled into the device (it may have been edited
// there), so the device only fills in what the server lacks and never overwrites a server value.
// The stream cannot tell us what the server has: with sendMeta it only delivers metadata together with
// a value, and writable paths often have none yet. So ask over HTTP, one subscriber per call.
void SignalK::sync_metadata() {
  for (auto *subscriber : sensors_) {
    if (!subscriber->needs_metadata_sync())
      continue;
    if (!subscriber->has_metadata() || subscriber->get_path().find('*') != std::string::npos) {
      subscriber->set_metadata_synced();  // nothing to send, or a wildcard has no single metadata
      continue;
    }
    if (!this->sync_metadata_of(subscriber)) {
      metadata_next_attempt_at_ = sk_millis() + METADATA_RETRY_MS;
    }
    return;
  }
}

bool SignalK::sync_metadata_of(SignalkSubscriber *subscriber) {
  const std::string path = subscriber->get_path();
  std::string url_path = path;
  std::replace(url_path.begin(), url_path.end(), '.', '/');

  auto response = get("/signalk/v1/api/vessels/self/" + url_path + "/meta");
  JsonDocument server_meta;  // stays null when the server has no metadata for the path
  if (response.status_code == 200) {
    DeserializationError error = deserializeJson(server_meta, response.body);
    if (error || !server_meta.is<JsonObject>()) {
      ESP_LOGW(TAG, "Unreadable metadata for %s from server, retrying later", path.c_str());
      return false;
    }
  } else if (response.status_code != 404) {
    ESP_LOGW(TAG, "Could not read metadata for %s: HTTP %d, retrying later", path.c_str(), response.status_code);
    return false;
  }

  JsonDocument missing;
  JsonObject missing_obj = missing.to<JsonObject>();
  subscriber->metadata_missing_from(server_meta.as<JsonVariantConst>(), missing_obj);
  if (missing_obj.size() == 0) {
    ESP_LOGD(TAG, "Server already has all metadata for %s", path.c_str());
    subscriber->set_metadata_synced();
    return true;
  }
  if (!this->publish_meta_delta(path, missing_obj)) {
    return false;
  }
  subscriber->set_metadata_synced();
  return true;
}

}  // namespace signalk
}  // namespace esphome
