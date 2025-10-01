
#pragma once
#include <string>
#include <cstdint>
#include <functional>

namespace esphome {
namespace signalk {

class SignalKClient {
 public:
  using ConnectedCallback = std::function<void()>;
  using MessageCallback = std::function<void(const std::string &)>;

  virtual ~SignalKClient() = default;
  virtual bool connect(const std::string &host, uint16_t port, const std::string &path) = 0;
  virtual bool discovery() = 0;
  virtual bool send(const std::string &msg) = 0;
  virtual void update() = 0;
  virtual bool is_connected() const = 0;

  virtual std::string get(const std::string &path) = 0;
  virtual std::string post(const std::string &path, const std::string &msg) = 0;

  virtual void on_message(MessageCallback cb) { message_callback_ = std::move(cb); }
  virtual void on_connected(ConnectedCallback cb) { connected_callback_ = std::move(cb); }
  virtual void on_disconnected(ConnectedCallback cb) { disconnected_callback_ = std::move(cb); }
  // virtual void loop() = 0;
 protected:
  MessageCallback message_callback_;
  ConnectedCallback connected_callback_;
  ConnectedCallback disconnected_callback_;
};

}  // namespace signalk
}  // namespace esphome
