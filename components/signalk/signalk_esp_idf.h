#ifdef USE_ESP32_FRAMEWORK_ESP_IDF
#pragma once
#include <atomic>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>

#include "signalk.h"
#include "signalk_mdns_esp32.h"
#include "esp_websocket_client.h"
#include "esp_http_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

namespace esphome {
namespace signalk {

esp_err_t _http_event_handler(esp_http_client_event_t *evt);

class SignalKEspIdf : public SignalK {
 public:
  SignalKEspIdf() { isconnected_ = false; }
  ~SignalKEspIdf() {}

  bool connect(const std::string &path) override;
  void disconnect() override;
  DiscoveryPoll poll_discovery(std::vector<ServerCandidate> &found) override;
  bool send(const std::string &msg) override;
  bool is_connected() const override { return isconnected_; }

  HttpResponse get(const std::string &path) override;
  HttpResponse post(const std::string &path, const std::string &msg) override;

  /// Runs in the esp_websocket_client task: only hands events and complete messages over.
  void handle_websocket_event(int32_t event_id, esp_websocket_event_data_t *data);

 private:
  /// The sender task: writes queued messages, so a slow link never blocks the main loop.
  static void sender_task_entry(void *arg);
  void sender_loop();
  void clear_outbox();

  SignalKMdnsEsp32 mdns_;
  esp_websocket_client_handle_t websocket_client{nullptr};
  bool isconnected_;
  std::string auth_header_;

  // Outgoing messages, from send() (main loop) to the sender task, oldest first.
  std::mutex outbox_mutex_;
  std::deque<std::string> outbox_;
  size_t outbox_bytes_{0};
  // Messages dropped (outbox full, or the write failed); reported from the main loop.
  std::atomic<uint32_t> outbox_dropped_{0};
  TaskHandle_t sender_task_{nullptr};
  // Held by the sender task while it writes, and by connect()/disconnect() while they
  // create or destroy the client, so a write never runs on a destroyed client.
  SemaphoreHandle_t client_mutex_{nullptr};

  // Reassembly of text messages delivered in several chunks (websocket task only).
  std::string frame_;
  bool frame_text_{false};
  bool frame_overflow_{false};
};

}  // namespace signalk
}  // namespace esphome

#endif
