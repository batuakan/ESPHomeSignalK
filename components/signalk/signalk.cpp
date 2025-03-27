
#include "esphome/components/signalk/signalk.h"
#include "esphome/core/log.h"

#include <ArduinoJson.h>

namespace esphome {
namespace signalk {
#ifdef USE_ESP32_FRAMEWORK_ARDUINO
using namespace websockets;
#endif

#if defined(USE_ESP_IDF)
SignalK *ws_client;

static void websocket_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
  esp_websocket_event_data_t *data = (esp_websocket_event_data_t *) event_data;
  switch (event_id) {
    case WEBSOCKET_EVENT_CONNECTED:
      ESP_LOGD(__FILENAME__, "WEBSOCKET_EVENT_CONNECTED");
      ws_client->on_connected();
      break;
    case WEBSOCKET_EVENT_DISCONNECTED:
      ESP_LOGD(__FILENAME__, "WEBSOCKET_EVENT_DISCONNECTED");
      // ws_client->on_disconnected();
      break;
    case WEBSOCKET_EVENT_DATA:
      // check if the payload is text)
      if (data->op_code == 0x01) {
        ws_client->on_receive_delta((uint8_t *) data->data_ptr, data->data_len);
      }
      break;
    case WEBSOCKET_EVENT_ERROR:
      // ws_client->on_error();
      break;
  }
}
#endif

static const char *const TAG = "signalk";

void SignalK::setup() {
  ESP_LOGD(TAG, "Setup");
#ifdef USE_ESP32_FRAMEWORK_ARDUINO
  // run callback when messages are received
  webSocketClient_.onMessage(
      [&](WebsocketsMessage message) { ESP_LOGD(TAG, "Got Message: %s", message.data().c_str()); });

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

  on_connected();
#endif
#if defined(USE_ESP_IDF)
  ws_client = this;
  const esp_websocket_client_config_t websocket_cfg = {
      .host = host_.c_str(), .port = port_, .path = "/signalk/v1/stream?subscribe=none"};

  websocket_client = esp_websocket_client_init(&websocket_cfg);

  esp_websocket_register_events(websocket_client, WEBSOCKET_EVENT_ANY, websocket_event_handler,
                                (void *) websocket_client);
  esp_websocket_client_start(websocket_client);
#endif
}

void SignalK::update() {
#ifdef USE_ESP32_FRAMEWORK_ARDUINO
  if (!connected) {
    // TODO: Handle millis() overflow properly
    unsigned long now = 0;  // millis();
    elapsed_duration += now - last_millis;
    last_millis = now;
    if (elapsed_duration > 5000) {
      on_connected();
    }
  } else {
    webSocketClient_.poll();
  }
#endif
}

void SignalK::dump_config() {}

void SignalK::on_connected() {
  elapsed_duration = 0;
  ESP_LOGD(TAG, "Attemping to connect to signalk server");
#ifdef USE_ESP32_FRAMEWORK_ARDUINO
  connected = webSocketClient_.connect(host_.c_str(), port_, "/signalk/v1/stream?subscribe=none");
  if (!connected) {
    ESP_LOGW(TAG, "Connection attemp failed!");
    return;
  }
#endif

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
  ESP_LOGD(TAG, "Subscription message:");
  ESP_LOGD(TAG, output.c_str());
#ifdef USE_ESP32_FRAMEWORK_ARDUINO
  webSocketClient_.send(output.c_str());
#endif
#if defined(USE_ESP_IDF)
  const TickType_t xDelay = 500;
  esp_websocket_client_send_text(websocket_client, output.c_str(), output.size(), xDelay);
#endif
}

void SignalK::on_receive_delta(uint8_t *payload, size_t length) {
  JsonDocument doc;
  deserializeJson(doc, payload);
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
    } else if (delta["value"].is<JsonArray>() || delta["value"].is<JsonObject>()) {
      std::string output;
      serializeJson(delta["value"], output);
      sensor->on_delta_received(output);
    }
  }
}

}  // namespace signalk
}  // namespace esphome
