#ifdef USE_HOST
#pragma once
#include <string>
#include <cstdint>

#include "signalk.h"
#include "signalk_mdns_host.h"

#include <ixwebsocket/IXWebSocket.h>

namespace esphome {
namespace signalk {

class SignalKx86 : public SignalK {
 public:
  SignalKx86() { isconnected_ = false; }
  ~SignalKx86() {}

  bool connect(const std::string &path) override;
  void disconnect() override;
  DiscoveryPoll poll_discovery(std::vector<ServerCandidate> &found) override;
  bool send(const std::string &msg) override;

  // void update() override {}

  // void connected();
  // void disconnected();
  // void on_receive_delta(const std::string &msg);
  bool is_connected() const override { return isconnected_; }

  HttpResponse get(const std::string &path) override;
  HttpResponse post(const std::string &path, const std::string &msg) override;

 private:
  /// Over TLS: true once the server's certificate is known to be the pinned one (fetched with OpenSSL on the
  /// first connection, and pinned then), so that ixwebsocket can trust exactly that certificate.
  bool ensure_certificate();
  SignalKMdnsHost mdns_;
  bool isconnected_;
  ix::WebSocket websocket_client_;
  // uint32_t content_length = 0;

  // void receive_chunk(const char *data, int data_len);
};

}  // namespace signalk
}  // namespace esphome

#endif
