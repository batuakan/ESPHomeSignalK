#pragma once
#include <map>

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"

#include <ArduinoWebsockets.h>
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

 protected:
  void connect();

  unsigned long last_millis;
  unsigned long elapsed_duration;
  bool connected;

  std::string host_;
  unsigned short port_;
  std::map<std::string, SignalkSensorBase *> sensors_;
  websockets::WebsocketsClient webSocketClient_;
};

}  // namespace signalk
}  // namespace esphome
