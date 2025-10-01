#ifdef USE_ESP_IDF
#pragma once
#include <string>
#include <cstdint>

#include "signalk.h"
#include "esp_websocket_client.h"
#include "esp_http_client.h"

namespace esphome {
namespace signalk {

static esp_err_t _http_event_handler(esp_http_client_event_t *evt);

class SignalKEspIdf : public SignalK {
  friend esp_err_t _http_event_handler(esp_http_client_event_t *evt);

 public:
  SignalKEspIdf() { isconnected_ = false; }
  ~SignalKEspIdf() {}

  bool connect(const std::string &path) override;
  bool discovery() override;
  bool send(const std::string &msg) override;
  bool is_connected() const override { return isconnected_; }

  HttpResponse get(const std::string &path) override;
  HttpResponse post(const std::string &path, const std::string &msg) override;

 private:
  esp_websocket_client_handle_t websocket_client;
  bool isconnected_;
  std::string auth_header_;
};

}  // namespace signalk
}  // namespace esphome

#endif
