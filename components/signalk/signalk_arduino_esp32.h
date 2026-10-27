#ifdef USE_ESP32_FRAMEWORK_ARDUINO
#pragma once
#include <string>
#include <cstdint>

#include "signalk.h"
#include "signalk_mdns_esp32.h"
#include <ArduinoWebsockets.h>

namespace esphome {
namespace signalk {

class SignalKArduino : public SignalK {
 public:
  SignalKArduino() {}
  ~SignalKArduino();
  bool connect(const std::string &path) override;
  DiscoveryPoll poll_discovery(std::vector<ServerCandidate> &found) override;
  bool send(const std::string &msg) override;
  bool is_connected() const override { return isconnected_; }

  HttpResponse get(const std::string &path) override;
  HttpResponse post(const std::string &path, const std::string &msg) override;

 private:
  SignalKMdnsEsp32 mdns_;
  websockets::WebsocketsClient webSocketClient_;
  bool isconnected_ = false;
  std::string auth_header_;
};

}  // namespace signalk
}  // namespace esphome
#endif
