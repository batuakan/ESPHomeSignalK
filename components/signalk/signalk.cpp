


#include "esphome/core/preferences.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include <regex>
// #include "esphome/components/wifi/wifi_component.h"

#include "signalk.h"
#include "utils.h"

namespace esphome {
namespace signalk {

static const char *const TAG = "signalk";

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
  if (is_connected())
  {
    for (auto it = sensors_.begin(); it != sensors_.end(); ++it) {
      if (it->second != NULL) {
        it->second->update();
      } 
    }
  }
}

void SignalK::dump_config() {}

void SignalK::on_connected() {
  JsonDocument doc;
  doc["context"] = "vessels.self";
  JsonArray subscribe = doc["subscribe"].to<JsonArray>();
  for (auto it = sensors_.begin(); it != sensors_.end(); ++it) {
    JsonObject subscription = subscribe.add<JsonObject>();
    subscription["path"] = it->second->get_path();
    subscription["period"] = it->second->get_period();
    subscription["format"] = it->second->get_format();
    subscription["policy"] = it->second->get_policy();
    subscription["minPeriod"] = it->second->get_min_period();
  }
  std::string output;
  serializeJsonPretty(doc, output);
  ESP_LOGD(TAG, "Subscription message: %s", output.c_str());
  send(output);
}

void SignalK::on_disconnected() {
  ESP_LOGW(TAG, "Disconnected from SignalK server");
  login_state_ = SignalKLoginState::UNKNOWN;
  if (!token_.empty())
    request_access_state_ = SignalKRequestAccessState::HASTOKEN;
  else
    request_access_state_ = SignalKRequestAccessState::UNKNOWN;
  for (auto it = sensors_.begin(); it != sensors_.end(); ++it) {
    it->second->disconnected();
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
  ESP_LOGD(TAG, "Trying to login: %s", msg.c_str());
  auto response = post("/signalk/v1/auth/login", msg);
  ESP_LOGD(TAG, "Login request response: %s", response.body.c_str());

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
  JsonDocument doc;
  doc["clientId"] = get_uuid();
  doc["description"] = "ESPHome device: " + App.get_name();
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
  ESP_LOGD(TAG, "Access request status response: %s", response.body.c_str());

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
      ESP_LOGI(TAG, "Access token: %s", token_.c_str());
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

  auto response = get("/signalk/v1/stream");
  ESP_LOGD(TAG, "Token validation response:  %d %s", response.status_code, response.body.c_str());
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
    ESP_LOGI(TAG, "Saved token to preferences: %s", token_.c_str());
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
      ESP_LOGI(TAG, "Loaded token: %s", token_.c_str());
      return;
    }
  } else {
    ESP_LOGW(TAG, "No token stored in flash");
  }
}

void SignalK::on_message(const std::string &msg) {
  ESP_LOGD(TAG, "Received message: %s", msg.c_str());
  JsonDocument doc;
  deserializeJson(doc, msg);
  // Check if this is a delta message
  JsonArray arr = doc["updates"][0]["values"].as<JsonArray>();
  if (!arr.isNull())
    this->on_receive_delta(arr);
  JsonArray put_arr = doc["put"].as<JsonArray>();
  if (!put_arr.isNull())
    this->on_receive_delta(put_arr);
  // Check if this a token validation message
}

void SignalK::on_receive_delta(JsonArray &arr) {
  // JsonArray arr = doc["updates"][0]["values"].as<JsonArray>();
  for (JsonVariant delta : arr) {
    if (!delta["path"].is<std::string>()) {
      continue;
    }
    auto path = delta["path"].as<std::string>();
    auto sensor = sensors_[path];
    if (sensor == NULL)
    {
      //Exact key match was not found, lets try regex matching
      for (auto it = sensors_.begin(); it != sensors_.end(); ++it) {
        if (std::regex_match(path, std::regex(it->first))) {
          set_sensor_value(it->second, delta["value"]);
        }
      }
    }
    if (sensor != NULL)
    {
      set_sensor_value(sensor, delta["value"]);
    }    
  }
}

void SignalK::set_sensor_value(SignalkSubscriber *sensor, JsonVariant value) {
  if (sensor == NULL) {
    return;
  }
  sensor->set_value(value);
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

void SignalK::publish_meta_delta(SignalkSubscriber *subscriber)
{
  if (!is_connected()) {
    return;
  }
  JsonDocument doc;
  JsonArray updates = doc["updates"].to<JsonArray>();

  JsonObject update = updates.add<JsonObject>();

  JsonArray metas = update["meta"].to<JsonArray>();
  JsonObject meta = metas.add<JsonObject>();
  meta["path"] = subscriber->get_path();
  JsonObject value = meta["value"].to<JsonObject>();
  subscriber->to_json(value);
  

  std::string output;
  serializeJson(doc, output);
  ESP_LOGD(TAG, "Publishing meta delta: %s", output.c_str());
  send(output);

}

}  // namespace signalk
}  // namespace esphome
