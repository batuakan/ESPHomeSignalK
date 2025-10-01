#ifdef USE_HOST
#pragma once
#include <string>
#include <cstdint>

#include "signalk.h"

#include <ixwebsocket/IXWebSocket.h>

namespace esphome {
namespace signalk {

class SignalKx86 : public SignalK {
 public:
  SignalKx86() { isconnected_ = false; }
  ~SignalKx86() {}

  bool connect(const std::string &path) override;
  bool discovery() override;
  bool send(const std::string &msg) override;

  void save_token() override;
  void load_token() override;
  // void update() override {}

  // void connected();
  // void disconnected();
  // void on_receive_delta(const std::string &msg);
  bool is_connected() const override { return isconnected_; }

  HttpResponse get(const std::string &path) override;
  HttpResponse post(const std::string &path, const std::string &msg) override;

 private:
  bool isconnected_;
  ix::WebSocket websocket_client_;
  // uint32_t content_length = 0;

  // void receive_chunk(const char *data, int data_len);
};

}  // namespace signalk
}  // namespace esphome

#endif
