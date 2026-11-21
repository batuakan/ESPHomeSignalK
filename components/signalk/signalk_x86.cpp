#ifdef USE_HOST

#include "signalk_x86.h"
#include "esphome/core/log.h"
#include <cstdio>
#include <ixwebsocket/IXHttpClient.h>
#include <netdb.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

namespace esphome {
namespace signalk {

static const char *const TAG = "signalk";
// Connecting and the handshake when the certificate is fetched.
static const int FETCH_TIMEOUT_S = 3;

/// Connects to `host`:`port` over TLS without verifying anything and returns the certificate the server
/// presents, as PEM and its SHA-256. The caller decides whether to trust it.
static bool fetch_certificate(const std::string &host, uint16_t port, std::string &pem, Fingerprint &fp) {
  addrinfo hints{};
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  addrinfo *res = nullptr;
  char service[6];
  snprintf(service, sizeof(service), "%u", static_cast<unsigned>(port));
  if (getaddrinfo(host.c_str(), service, &hints, &res) != 0 || res == nullptr)
    return false;
  int fd = -1;
  for (addrinfo *a = res; a != nullptr && fd < 0; a = a->ai_next) {
    fd = socket(a->ai_family, a->ai_socktype, a->ai_protocol);
    if (fd < 0)
      continue;
    timeval tv{FETCH_TIMEOUT_S, 0};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    if (::connect(fd, a->ai_addr, a->ai_addrlen) != 0) {
      ::close(fd);
      fd = -1;
    }
  }
  freeaddrinfo(res);
  if (fd < 0)
    return false;

  bool ok = false;
  SSL_CTX *ctx = SSL_CTX_new(TLS_client_method());
  SSL *ssl = ctx != nullptr ? SSL_new(ctx) : nullptr;
  if (ssl != nullptr) {
    SSL_set_verify(ssl, SSL_VERIFY_NONE, nullptr);
    SSL_set_fd(ssl, fd);
    if (SSL_connect(ssl) == 1) {
      if (X509 *cert = SSL_get1_peer_certificate(ssl)) {
        unsigned int n = 0;
        BIO *bio = BIO_new(BIO_s_mem());
        if (X509_digest(cert, EVP_sha256(), fp.data(), &n) == 1 && n == fp.size() && bio != nullptr &&
            PEM_write_bio_X509(bio, cert) == 1) {
          char *data = nullptr;
          long len = BIO_get_mem_data(bio, &data);
          pem.assign(data, len > 0 ? (size_t) len : 0);
          ok = !pem.empty();
        }
        BIO_free(bio);
        X509_free(cert);
      }
    }
    SSL_free(ssl);
  }
  SSL_CTX_free(ctx);
  ::close(fd);
  return ok;
}

bool SignalKx86::ensure_certificate() {
  if (!tls_ || !tls_trust().ca_pem().empty())
    return true;
  std::string pem;
  Fingerprint fp{};
  if (!fetch_certificate(host_, port_, pem, fp)) {
    ESP_LOGW(TAG, "Could not read the certificate of %s:%u", host_.c_str(), port_);
    return false;
  }
  if (!tls_trust().check(fp))
    return false;  // reported from the main loop (take_refused)
  tls_trust().set_ca_pem(std::move(pem));
  return true;
}

/// Trusts only the pinned certificate (it is self-signed: it is its own CA). Its name is "localhost".
static ix::SocketTLSOptions pinned_tls_options() {
  ix::SocketTLSOptions options;
  options.caFile = tls_trust().ca_pem();
  options.disable_hostname_validation = true;
  return options;
}

bool SignalKx86::connect(const std::string &path) {
  ESP_LOGD(TAG, "Connect (ix::WebSocket)");
  if (!ensure_certificate())
    return false;
  char port_buf[6];
  snprintf(port_buf, sizeof(port_buf), "%u", static_cast<unsigned>(port_));
  std::string url = (tls_ ? "wss://" : "ws://") + host_ + ":" + port_buf + path;
  websocket_client_.setUrl(url);
  if (tls_)
    websocket_client_.setTLSOptions(pinned_tls_options());

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
      this->signal_connected();

    } else if (msg->type == ix::WebSocketMessageType::Close) {
      ESP_LOGD(TAG, "WebSocket disconnected");
      isconnected_ = false;
      this->signal_disconnected();
    } else if (msg->type == ix::WebSocketMessageType::Message) {
      std::string text = msg->str;
      this->enqueue_message(std::move(text));
    } else if (msg->type == ix::WebSocketMessageType::Error) {
      isconnected_ = false;  // ix::WebSocket is async; assume true for now
      ESP_LOGW(TAG, "WebSocket error: %s", msg->errorInfo.reason.c_str());
      this->signal_disconnected();
    }
  });

  websocket_client_.start();

  ESP_LOGD(TAG, "Connected (ix::WebSocket)");
  return false;
  ;
}

void SignalKx86::disconnect() {
  websocket_client_.stop();
  isconnected_ = false;
  this->signal_disconnected();
}

DiscoveryPoll SignalKx86::poll_discovery(std::vector<ServerCandidate> &found) {
  return mdns_.poll(found, search_tls());
}

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
  if (!ensure_certificate())
    return HttpResponse{-1, ""};
  ix::HttpClient http_client;
  if (tls_)
    http_client.setTLSOptions(pinned_tls_options());
  std::string url = base_url() + path;

  ix::HttpRequestArgsPtr args = http_client.createRequest();

  // Custom headers can be set
  ix::WebSocketHttpHeaders headers;
  if (!token_.empty()) {
    auto auth = "Bearer " + token_;
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
  if (!ensure_certificate())
    return HttpResponse{-1, ""};
  ix::HttpClient http_client;
  if (tls_)
    http_client.setTLSOptions(pinned_tls_options());
  std::string url = base_url() + path;
  ix::HttpRequestArgsPtr args = http_client.createRequest();
  // Custom headers can be set
  ix::WebSocketHttpHeaders headers;
  if (!token_.empty()) {
    auto auth = "Bearer " + token_;
    headers["Authorization"] = auth;
  }
  args->extraHeaders = headers;

  auto response = http_client.get(url, args);
  HttpResponse res;
  res.status_code = response->statusCode;
  res.body = response->body;
  return res;
}

}  // namespace signalk
}  // namespace esphome
#endif
