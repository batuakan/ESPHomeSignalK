#ifdef USE_HOST

#include "signalk_x86.h"
#include "esphome/core/log.h"
#include <cstdio>
#include <fstream>
#include <ixwebsocket/IXHttpClient.h>

namespace esphome {
namespace signalk {

static const char *const TAG = "signalk";

bool SignalKx86::connect(const std::string &path) {
  ESP_LOGD(TAG, "Connect (ix::WebSocket)");
  char port_buf[6];
  snprintf(port_buf, sizeof(port_buf), "%u", static_cast<unsigned>(port_));
  std::string url = "ws://" + host_ + ":" + port_buf + path;
  websocket_client_.setUrl(url);

  // Set Authorization header if token is present
  if (!token_.empty()) {
    ix::WebSocketHttpHeaders headers;
    headers["Authorization"] = "Bearer " + token_;
    websocket_client_.setExtraHeaders(headers);
  }

  websocket_client_.setOnMessageCallback([this](const ix::WebSocketMessagePtr &msg) {
    if (msg->type == ix::WebSocketMessageType::Open) {
      ESP_LOGD(TAG, "WebSocket connected");
      isconnected_ = true;  // ix::WebSocket is async; assume true for now
      this->on_connected();

    } else if (msg->type == ix::WebSocketMessageType::Close) {
      ESP_LOGD(TAG, "WebSocket disconnected");
      isconnected_ = false;
      this->on_disconnected();
    } else if (msg->type == ix::WebSocketMessageType::Message) {
      this->on_message(msg->str);
    } else if (msg->type == ix::WebSocketMessageType::Error) {
      isconnected_ = false;  // ix::WebSocket is async; assume true for now
      ESP_LOGW(TAG, "WebSocket error: %s", msg->errorInfo.reason.c_str());
    }
  });

  websocket_client_.start();

  ESP_LOGD(TAG, "Connected (ix::WebSocket)");
  return false;
  ;
}

bool SignalKx86::discovery() { return true; }

bool SignalKx86::send(const std::string &msg) {
  if (websocket_client_.getReadyState() != ix::ReadyState::Open) {
    ESP_LOGW(TAG, "WebSocket client not connected, cannot send message");
    return false;
  }
  websocket_client_.send(msg);
  return true;
}

HttpResponse SignalKx86::post(const std::string &path, const std::string &msg) {
  // This is a stub implementation using ix::WebSocket for x86 (not ESP-IDF).
  // ix::WebSocket does not support HTTP POST, so we use ix::HttpClient for HTTP requests.
  ix::HttpClient http_client;
  char port_buf[6];
  snprintf(port_buf, sizeof(port_buf), "%u", static_cast<unsigned>(port_));
  std::string url = "http://" + host_ + ":" + port_buf + path;

  ix::HttpRequestArgsPtr args = http_client.createRequest();

  // Custom headers can be set
  ix::WebSocketHttpHeaders headers;
  if (!token_.empty()) {
    auto auth = "Bearer " + token_;
    ESP_LOGD(TAG, "Using auth: %s", auth.c_str());
    headers["Authorization"] = auth;
  }
  headers["Content-Type"] = "application/json";
  args->extraHeaders = headers;

  auto response = http_client.post(url, msg, args);
  HttpResponse res;
  res.status_code = response->statusCode;
  res.body = response->body;
  return res;
}

HttpResponse SignalKx86::get(const std::string &path) {
  ix::HttpClient http_client;
  char port_buf[6];
  snprintf(port_buf, sizeof(port_buf), "%u", static_cast<unsigned>(port_));
  std::string url = "http://" + host_ + ":" + port_buf + path;
  ix::HttpRequestArgsPtr args = http_client.createRequest();
  // Custom headers can be set
  ix::WebSocketHttpHeaders headers;
  if (!token_.empty()) {
    auto auth = "Bearer " + token_;
    ESP_LOGD(TAG, "Using auth: %s", auth.c_str());
    headers["Authorization"] = auth;
  }
  args->extraHeaders = headers;

  auto response = http_client.get(url, args);
  HttpResponse res;
  res.status_code = response->statusCode;
  res.body = response->body;
  return res;
}

void SignalKx86::save_token() {
  constexpr size_t TOKEN_MAX_LENGTH = 256;
  const char *filename = "token";  // or absolute path if you prefer

  std::ofstream file(filename, std::ios::out | std::ios::trunc);
  if (!file.is_open()) {
    ESP_LOGE(TAG, "Failed to open %s for writing", filename);
    return;
  }

  // Clamp to max length - 1 to leave room for null terminator if needed
  size_t len = std::min(token_.size(), TOKEN_MAX_LENGTH - 1);
  file.write(token_.c_str(), len);
  file.close();

  ESP_LOGI(TAG, "Saved token to file '%s': %s", filename, token_.c_str());
}

void SignalKx86::load_token() {
  constexpr size_t TOKEN_MAX_LENGTH = 256;
  const char *filename = "token";

  std::ifstream file(filename, std::ios::in);
  if (!file.is_open()) {
    ESP_LOGW(TAG, "No token file found (%s)", filename);
    return;
  }

  char buffer[TOKEN_MAX_LENGTH] = {0};
  file.read(buffer, TOKEN_MAX_LENGTH - 1);
  file.close();

  token_ = std::string(buffer);
  if (!token_.empty()) {
    request_access_state_ = SignalKRequestAccessState::HASTOKEN;
    ESP_LOGI(TAG, "Loaded token from file '%s': %s", filename, token_.c_str());
  } else {
    ESP_LOGW(TAG, "Token file was empty");
  }
}

}  // namespace signalk
}  // namespace esphome
#endif
