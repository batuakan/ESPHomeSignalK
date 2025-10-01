#ifdef USE_ESP32_FRAMEWORK_ARDUINO
#include "esphome/components/signalk/signalk_arduino_esp32.h"
#include "esphome/core/log.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <ESPmDNS.h>
namespace esphome {
namespace signalk {

static const char *const TAG = "signalk";
using namespace websockets;

bool SignalKArduino::connect(const std::string &path) {
  // run callback when messages are received
  webSocketClient_.onMessage([&](WebsocketsMessage message) {
    ESP_LOGD(TAG, "Got Message: %s", message.data().c_str());
    on_message(std::string(message.data().c_str(), message.data().length()));
  });

  // run callback when events are occuring
  webSocketClient_.onEvent([&](WebsocketsEvent event, String data) {
    if (event == WebsocketsEvent::ConnectionOpened) {
      ESP_LOGD(TAG, "Connnection Opened");
      isconnected_ = true;
      on_connected();
    } else if (event == WebsocketsEvent::ConnectionClosed) {
      ESP_LOGD(TAG, "Connnection Closed");
      isconnected_ = false;
      on_disconnected();
    } else if (event == WebsocketsEvent::GotPing) {
      ESP_LOGD(TAG, "Got a Ping!");
    } else if (event == WebsocketsEvent::GotPong) {
      ESP_LOGD(TAG, "Got a Pong!");
    }
  });

  if (!token_.empty()) {
    auth_header_ = "Bearer " + token_;
    webSocketClient_.addHeader("Authorization", auth_header_.c_str());
  }

  // Connect with custom headers
  isconnected_ = webSocketClient_.connect(host_.c_str(), port_, "/signalk/v1/stream?subscribe=none");
  return isconnected_;
}

bool SignalKArduino::send(const std::string &msg) {
  webSocketClient_.send(msg.c_str(), msg.length());
  return true;
}

bool SignalKArduino::discovery() {
  if (!MDNS.begin("esp32")) {  // start mDNS responder with some hostname
    Serial.println("Error starting mDNS");
    return false;
  }

  Serial.println("mDNS started, querying for _signalk._http._tcp services...");

  int n = MDNS.queryService("_signalk", "_http");  // service, proto
  if (n == 0) {
    Serial.println("No SignalK server found");
    return false;
  }

  for (int i = 0; i < n; i++) {
    String host = MDNS.hostname(i);
    int port = MDNS.port(i);

    Serial.printf("Found SignalK service: %s:%d\n", host.c_str(), port);

    // Save discovered host and port
    host_ = host.c_str();
    port_ = port;
  }

  return true;
}

HttpResponse SignalKArduino::post(const std::string &path, const std::string &msg) {
  HttpResponse resp;

  String url = "http://" + String(host_.c_str()) + ":" + String(port_) + String(path.c_str());
  HTTPClient http;

  if (!http.begin(url)) {
    resp.status_code = -1;
    resp.body = "Failed to init HTTP client";
    return resp;
  }

  if (!token_.empty()) {
    auth_header_ = "Bearer " + token_;
    http.addHeader("Authorization", auth_header_.c_str());
  }

  http.addHeader("Content-Type", "application/json");

  int httpCode = http.POST((uint8_t *) msg.c_str(), msg.length());

  resp.status_code = httpCode;
  if (httpCode > 0) {
    resp.body = http.getString().c_str();
  } else {
    resp.body = "HTTP POST failed: " + std::string(http.errorToString(httpCode).c_str());
  }

  http.end();
  return resp;
}

HttpResponse SignalKArduino::get(const std::string &path) {
  HttpResponse resp;

  String url = "http://" + String(host_.c_str()) + ":" + String(port_) + String(path.c_str());
  HTTPClient http;

  if (!http.begin(url)) {
    resp.status_code = -1;
    resp.body = "Failed to init HTTP client";
    return resp;
  }

  if (!token_.empty()) {
    auth_header_ = "Bearer " + token_;
    http.addHeader("Authorization", auth_header_.c_str());
  }

  int httpCode = http.GET();

  resp.status_code = httpCode;
  if (httpCode > 0) {
    resp.body = http.getString().c_str();
  } else {
    resp.body = "HTTP GET failed: " + std::string(http.errorToString(httpCode).c_str());
  }

  http.end();
  return resp;
}

}  // namespace signalk
}  // namespace esphome
#endif
