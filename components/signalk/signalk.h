#pragma once
#include <map>

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"

// #include "WiFiClientSecure.h"
// #include "WebSocketsClient.h"

#ifdef USE_ESP32_FRAMEWORK_ARDUINO
#include <ArduinoWebsockets.h>
#endif

#if defined(USE_ESP_IDF)
#include "esp_websocket_client.h"
#endif

#include "signalk_sensor_base.h"

namespace esphome {
namespace signalk {

class SignalK : public PollingComponent {
 public:
  void dump_config() override;
  void setup() override;
  void update() override;

  void set_host(std::string host) { host_ = host; }
  void set_port(unsigned short port) { port_ = port; }
  void subscribe(SignalkSensorBase *sensor) {
    sensors_.insert(std::pair<std::string, SignalkSensorBase *>(sensor->get_path(), sensor));
  }

  void on_connected();
  void on_receive_delta(uint8_t *payload, size_t length);

 protected:
  unsigned long last_millis;
  unsigned long elapsed_duration;
  bool connected;

  std::string host_;
  unsigned short port_;
  std::map<std::string, SignalkSensorBase *> sensors_;
#ifdef USE_ESP32_FRAMEWORK_ARDUINO
  websockets::WebsocketsClient webSocketClient_;
#endif
#if defined(USE_ESP_IDF)
  esp_websocket_client_handle_t websocket_client;
#endif
};

}  // namespace signalk
}  // namespace esphome
