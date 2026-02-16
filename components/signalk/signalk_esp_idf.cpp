#ifdef USE_ESP_IDF
#include "mdns.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_http_client.h"
#include <cstdio>

#include "esphome/components/signalk/signalk_esp_idf.h"
#include "esphome/core/log.h"

namespace esphome {
namespace signalk {

static const char *const TAG = "signalk";

static void websocket_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
  auto self = static_cast<SignalKEspIdf *>(handler_args);
  esp_websocket_event_data_t *data = (esp_websocket_event_data_t *) event_data;
  switch (event_id) {
    case WEBSOCKET_EVENT_CONNECTED:
      ESP_LOGD(__FILENAME__, "WEBSOCKET_EVENT_CONNECTED");
      self->on_connected();
      break;
    case WEBSOCKET_EVENT_DISCONNECTED:
      ESP_LOGD(__FILENAME__, "WEBSOCKET_EVENT_DISCONNECTED");
      self->on_disconnected();
      break;
    case WEBSOCKET_EVENT_DATA:
      // check if the payload is text)
      if (data->op_code == 0x01) {
        self->on_message(std::string(data->data_ptr, data->data_len));
      }
      break;
    case WEBSOCKET_EVENT_ERROR:
      // ws_client->on_error();
      break;
  }
}

// Optional callback for streaming/chunked response
static esp_err_t _http_event_handler(esp_http_client_event_t *evt) {
  static char *output_buffer;  // Buffer to store response of http request from event handler
  static int output_len;       // Stores number of bytes read

  switch (evt->event_id) {
    case HTTP_EVENT_ON_DATA:
      // ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
      /*
       *  Check for chunked encoding is added as the URL for chunked encoding used in this example returns binary data.
       *  However, event handler can also be used in case chunked encoding is used.
       */
      if (!esp_http_client_is_chunked_response(evt->client)) {
        // If user_data buffer is configured, copy the response into the buffer
        if (evt->user_data) {
          memcpy((char *) evt->user_data + output_len, evt->data, evt->data_len);
        } else {
          if (output_buffer == NULL) {
            output_buffer = (char *) malloc(esp_http_client_get_content_length(evt->client));
            output_len = 0;
            if (output_buffer == NULL) {
              ESP_LOGE(TAG, "Failed to allocate memory for output buffer");
              return ESP_FAIL;
            }
          }
          memcpy(output_buffer + output_len, evt->data, evt->data_len);
        }
        output_len += evt->data_len;
      }

      break;
    case HTTP_EVENT_ON_FINISH:
      // ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
      if (output_buffer != NULL) {
        // Response is accumulated in output_buffer. Uncomment the below line to print the accumulated response
        // ESP_LOG_BUFFER_HEX(TAG, output_buffer, output_len);
        free(output_buffer);
        output_buffer = NULL;
      }
      output_len = 0;
      break;
    case HTTP_EVENT_DISCONNECTED:
      // ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
      // int mbedtls_err = 0;
      // esp_err_t err = esp_tls_get_and_clear_last_error(evt->data, &mbedtls_err, NULL);
      // if (err != 0) {
      //   ESP_LOGI(TAG, "Last esp error code: 0x%x", err);
      //   ESP_LOGI(TAG, "Last mbedtls failure: 0x%x", mbedtls_err);
      // }
      if (output_buffer != NULL) {
        free(output_buffer);
        output_buffer = NULL;
      }
      output_len = 0;
      break;
    default:
      break;
  }
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

bool SignalKEspIdf::discovery() {
  mdns_result_t *results = nullptr;
  esp_err_t err = mdns_query_ptr("_signalk", "_http._tcp", 3000, 20, &results);

  if (err) {
    ESP_LOGE(TAG, "mDNS query failed: %s", esp_err_to_name(err));
    return false;
  }

  if (!results) {
    ESP_LOGW(TAG, "No SignalK server found");
    return false;
  }

  for (mdns_result_t *r = results; r; r = r->next) {
    ESP_LOGI(TAG, "Found service at %s:%u", r->hostname, r->port);
    // Save hostname/port and use it to connect
  }

  mdns_query_results_free(results);
  return true;
}

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
  char responseBuffer[512] = {0};
  char port_buf[6];
  snprintf(port_buf, sizeof(port_buf), "%u", static_cast<unsigned>(port_));
  std::string url = "http://" + host_ + ":" + port_buf + path;
  esp_http_client_config_t config = {
      .url = url.c_str(),
      .method = HTTP_METHOD_POST,
      .timeout_ms = 2000,  // important!
      .event_handler = _http_event_handler,
      .user_data = responseBuffer,

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
  resp.body = std::string(responseBuffer);
  return resp;
}

HttpResponse SignalKEspIdf::get(const std::string &path) {
  char responseBuffer[1024] = {0};
  char port_buf[6];
  snprintf(port_buf, sizeof(port_buf), "%u", static_cast<unsigned>(port_));
  std::string url = "http://" + host_ + ":" + port_buf + path;
  ESP_LOGW(TAG, "Connecting to %s", url.c_str());
  esp_http_client_config_t config = {
      .url = url.c_str(),
      .method = HTTP_METHOD_GET,
      .timeout_ms = 2000,  // important!
      .event_handler = _http_event_handler,
      .user_data = responseBuffer,
  };

  esp_http_client_handle_t client = esp_http_client_init(&config);
  if (token_.empty()) {
    ESP_LOGW(TAG, "No authorization token provided");
  } else {
    auth_header_ = "Bearer " + token_;
    ESP_LOGW(TAG, "Auth header: %s", auth_header_.c_str());
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
  resp.body = std::string(responseBuffer);
  return resp;
}

}  // namespace signalk
}  // namespace esphome
#endif
