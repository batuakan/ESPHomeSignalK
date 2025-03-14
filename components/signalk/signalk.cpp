
#include "esphome/components/signalk/signalk.h"
#include "esphome/core/log.h"

#include <ArduinoJson.h>

namespace esphome {
namespace signalk {

using namespace websockets;

static const char *const TAG = "signalk";

void SignalK::setup() {
  ESP_LOGD(TAG, "Setup");
  // run callback when messages are received
  webSocketClient_.onMessage([&](WebsocketsMessage message) {
    ESP_LOGD(TAG, "Got Message: %s", message.data().c_str());

    JsonDocument doc;
    deserializeJson(doc, message.data());
    JsonArray arr = doc["updates"][0]["values"].as<JsonArray>();
    if (arr.isNull())
      return;

    for (JsonVariant delta : arr) {
      if (!delta["path"].is<std::string>()) {
        continue;
      }
      auto path = delta["path"].as<std::string>();
      auto sensor = sensors_[path];
      if (delta["value"].is<double>()) {
        sensor->on_delta_received(delta["value"].as<double>());
      } else if (delta["value"].is<std::string>()) {
        sensor->on_delta_received(delta["value"].as<std::string>());
      }
    }
  });

  // run callback when events are occuring
  webSocketClient_.onEvent([&](WebsocketsEvent event, String data) {
    if (event == WebsocketsEvent::ConnectionOpened) {
      ESP_LOGD(TAG, "Connnection Opened");
    } else if (event == WebsocketsEvent::ConnectionClosed) {
      ESP_LOGD(TAG, "Connnection Closed");
      connected = false;
      last_millis = millis();
    } else if (event == WebsocketsEvent::GotPing) {
      ESP_LOGD(TAG, "Got a Ping!");
    } else if (event == WebsocketsEvent::GotPong) {
      ESP_LOGD(TAG, "Got a Pong!");
    }
  });

  connect();
}

void SignalK::update() {
  if (!connected) {
    // TODO: Handle millis() overflow properly
    unsigned long now = millis();
    elapsed_duration += now - last_millis;
    last_millis = now;
    if (elapsed_duration > 5000) {
      connect();
    }
  } else {
    webSocketClient_.poll();
  }
}

void SignalK::dump_config() {}

void SignalK::connect() {
  elapsed_duration = 0;
  ESP_LOGD(TAG, "Attemping to connect to signalk server");
  connected = webSocketClient_.connect(host_.c_str(), port_, "/signalk/v1/stream?subscribe=none");
  if (!connected) {
    ESP_LOGW(TAG, "Connection attemp failed!");
    return;
  }
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
  String output;
  serializeJson(doc, output);
  ESP_LOGD(TAG, "Subscription message:");
  ESP_LOGD(TAG, output.c_str());
  webSocketClient_.send(output);
}

}  // namespace signalk
}  // namespace esphome
