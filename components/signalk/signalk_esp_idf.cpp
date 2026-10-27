#ifdef USE_ESP32_FRAMEWORK_ESP_IDF
#include "esp_log.h"
#include "esp_system.h"
#include "esp_http_client.h"
#include <algorithm>
#include <cstdio>

#include "esphome/components/signalk/signalk_esp_idf.h"
#include "esphome/core/log.h"

namespace esphome {
namespace signalk {

static const char *const TAG = "signalk";

// Largest text message reassembled from chunks; bigger ones are dropped with a warning.
static const size_t MAX_MESSAGE_BYTES = 32 * 1024;
// Largest HTTP response body kept by get()/post() (access requests, login, validation).
static const size_t MAX_HTTP_RESPONSE_BYTES = 4096;

static void websocket_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
  static_cast<SignalKEspIdf *>(handler_args)
      ->handle_websocket_event(event_id, static_cast<esp_websocket_event_data_t *>(event_data));
}

void SignalKEspIdf::handle_websocket_event(int32_t event_id, esp_websocket_event_data_t *data) {
  switch (event_id) {
    case WEBSOCKET_EVENT_CONNECTED:
      this->signal_connected();
      break;
    case WEBSOCKET_EVENT_DISCONNECTED:
      this->signal_disconnected();
      break;
    case WEBSOCKET_EVENT_DATA: {
      // A message larger than the client buffer arrives as several events with the same
      // payload_len and increasing payload_offset; the first carries the text op code.
      if (data->payload_offset == 0) {
        this->frame_.clear();
        this->frame_text_ = data->op_code == 0x01;
        this->frame_overflow_ = data->payload_len > (int) MAX_MESSAGE_BYTES;
      }
      if (!this->frame_text_)
        break;
      if (!this->frame_overflow_)
        this->frame_.append(data->data_ptr, data->data_len);
      if (data->payload_offset + data->data_len >= data->payload_len) {
        if (this->frame_overflow_) {
          ESP_LOGW(TAG, "Dropping %d byte message (limit %u)", data->payload_len, (unsigned) MAX_MESSAGE_BYTES);
        } else {
          this->enqueue_message(std::move(this->frame_));
        }
        this->frame_.clear();
        this->frame_text_ = false;
      }
      break;
    }
    case WEBSOCKET_EVENT_ERROR:
      break;
  }
}

/// Response accumulator for esp_http_client, bounded to MAX_HTTP_RESPONSE_BYTES.
struct ResponseBuffer {
  std::string body;
  bool truncated{false};
};

esp_err_t _http_event_handler(esp_http_client_event_t *evt) {
  if (evt->event_id != HTTP_EVENT_ON_DATA || evt->user_data == nullptr)
    return ESP_OK;
  auto *buf = static_cast<ResponseBuffer *>(evt->user_data);
  size_t room = MAX_HTTP_RESPONSE_BYTES - std::min(buf->body.size(), MAX_HTTP_RESPONSE_BYTES);
  size_t take = std::min(room, (size_t) evt->data_len);
  buf->body.append(static_cast<const char *>(evt->data), take);
  if (take < (size_t) evt->data_len)
    buf->truncated = true;
  return ESP_OK;
}

bool SignalKEspIdf::connect(const std::string &path) {
  ESP_LOGD(TAG, "Connect");
  esp_websocket_client_config_t websocket_cfg = {};
  websocket_cfg.host = host_.c_str();
  websocket_cfg.port = port_;
  websocket_cfg.path = path.c_str();
  // websocket_cfg.headers = ... // Not available in IDF 4.x, see below

  // Set Authorization header if token is present
  if (!token_.empty()) {
    // Note: websocket_cfg.headers is only available in ESP-IDF v4.1+.
    // If using an older version, you may need to upgrade or patch the IDF.
    // Also, ensure that auth_token_ is a member variable containing the token.
    auth_header_ = "Authorization: Bearer " + token_ + "\r\n";
    websocket_cfg.headers = auth_header_.c_str();
  }

  websocket_client = esp_websocket_client_init(&websocket_cfg);

  esp_websocket_register_events(websocket_client, WEBSOCKET_EVENT_ANY, websocket_event_handler, this);
  isconnected_ = esp_websocket_client_start(websocket_client) == ESP_OK;
  ESP_LOGD(TAG, "Connected");
  return isconnected_;
}

DiscoveryPoll SignalKEspIdf::poll_discovery(std::vector<ServerCandidate> &found) { return mdns_.poll(found); }

bool SignalKEspIdf::send(const std::string &msg) {
  if (!esp_websocket_client_is_connected(websocket_client)) {
    ESP_LOGW(TAG, "WebSocket client not connected, cannot send message");
    return false;
  }
  const TickType_t xDelay = 500;
  esp_websocket_client_send_text(websocket_client, msg.c_str(), msg.size(), xDelay);
  return true;
}

HttpResponse SignalKEspIdf::post(const std::string &path, const std::string &msg) {
  ResponseBuffer responseBuffer;
  char port_buf[6];
  snprintf(port_buf, sizeof(port_buf), "%u", static_cast<unsigned>(port_));
  std::string url = "http://" + host_ + ":" + port_buf + path;
  esp_http_client_config_t config = {
      .url = url.c_str(),
      .method = HTTP_METHOD_POST,
      .timeout_ms = 2000,  // important!
      .event_handler = _http_event_handler,
      .user_data = &responseBuffer,

  };

  esp_http_client_handle_t client = esp_http_client_init(&config);
  // if (auth.empty()) {
  //   ESP_LOGW(TAG, "No authorization token provided");
  // } else {
  //   esp_http_client_set_header(client, "Authorization", auth.c_str());
  // }
  esp_http_client_set_header(client, "Content-Type", "application/json");
  esp_http_client_set_post_field(client, msg.c_str(), msg.length());
  esp_err_t err = esp_http_client_perform(client);

  HttpResponse resp;
  resp.status_code = esp_http_client_get_status_code(client);
  // this->content_length = esp_http_client_get_content_length(client);

  // ESP_LOGI(TAG, "POST Status = %d, content_length = %d", status, this->content_length);
  // ESP_LOGI(TAG, "POST Response = %s", responseBuffer);
  esp_http_client_cleanup(client);
  if (responseBuffer.truncated)
    ESP_LOGW(TAG, "HTTP response for %s truncated to %u bytes", path.c_str(), (unsigned) MAX_HTTP_RESPONSE_BYTES);
  resp.body = std::move(responseBuffer.body);
  return resp;
}

HttpResponse SignalKEspIdf::get(const std::string &path) {
  ResponseBuffer responseBuffer;
  char port_buf[6];
  snprintf(port_buf, sizeof(port_buf), "%u", static_cast<unsigned>(port_));
  std::string url = "http://" + host_ + ":" + port_buf + path;
  ESP_LOGD(TAG, "GET %s", url.c_str());
  esp_http_client_config_t config = {
      .url = url.c_str(),
      .method = HTTP_METHOD_GET,
      .timeout_ms = 2000,  // important!
      .event_handler = _http_event_handler,
      .user_data = &responseBuffer,
  };

  esp_http_client_handle_t client = esp_http_client_init(&config);
  if (token_.empty()) {
    ESP_LOGW(TAG, "No authorization token provided");
  } else {
    auth_header_ = "Bearer " + token_;
    esp_http_client_set_header(client, "Authorization", auth_header_.c_str());
  }
  esp_err_t err = esp_http_client_perform(client);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "HTTP GET failed: %s", esp_err_to_name(err));
  }

  HttpResponse resp;
  resp.status_code = esp_http_client_get_status_code(client);
  // this->content_length = esp_http_client_get_content_length(client);

  // ESP_LOGD(TAG, "GET Status = %d, content_length = %d", status, this->content_length);
  // ESP_LOGD(TAG, "GET Response = %s", responseBuffer);
  esp_http_client_cleanup(client);
  if (responseBuffer.truncated)
    ESP_LOGW(TAG, "HTTP response for %s truncated to %u bytes", path.c_str(), (unsigned) MAX_HTTP_RESPONSE_BYTES);
  resp.body = std::move(responseBuffer.body);
  return resp;
}

}  // namespace signalk
}  // namespace esphome
#endif
