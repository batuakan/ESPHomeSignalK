#ifdef USE_ESP32_FRAMEWORK_ESP_IDF
#pragma once
#include <string>
#include <cstdint>

#include "signalk.h"
#include "esp_websocket_client.h"
#include "esp_http_client.h"

namespace esphome {
namespace signalk {

esp_err_t _http_event_handler(esp_http_client_event_t *evt);

class SignalKEspIdf : public SignalK {
 public:
  SignalKEspIdf() { isconnected_ = false; }
  ~SignalKEspIdf() {}

  bool connect(const std::string &path) override;
  bool discovery() override;
  bool send(const std::string &msg) override;
  bool is_connected() const override { return isconnected_; }

  HttpResponse get(const std::string &path) override;
  HttpResponse post(const std::string &path, const std::string &msg) override;

  /// Runs in the esp_websocket_client task: only hands events and complete messages over.
  void handle_websocket_event(int32_t event_id, esp_websocket_event_data_t *data);

 private:
  esp_websocket_client_handle_t websocket_client;
  bool isconnected_;
  std::string auth_header_;

  // Reassembly of text messages delivered in several chunks (websocket task only).
  std::string frame_;
  bool frame_text_{false};
  bool frame_overflow_{false};
};

}  // namespace signalk
}  // namespace esphome

#endif
