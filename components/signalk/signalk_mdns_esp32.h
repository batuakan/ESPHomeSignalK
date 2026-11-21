#pragma once
#ifdef USE_ESP32

#include <vector>

#include "server_discovery.h"

namespace esphome {
namespace signalk {

/// Looks for SignalK servers with ESP-IDF's mDNS client (the same one ESPHome's mdns component starts, for
/// both the Arduino and the ESP-IDF framework). Never blocks: poll() starts an asynchronous search and
/// returns PENDING until it has finished.
class SignalKMdnsEsp32 {
 public:
  ~SignalKMdnsEsp32();
  /// `tls`: also look for servers with SSL (_signalk-https), in a second search alongside.
  DiscoveryPoll poll(std::vector<ServerCandidate> &found, bool tls);

 private:
  // mdns_search_once_t *, kept opaque so this header needs no ESP-IDF includes. Both run at once; a
  // finished one is collected into found_ and set to nullptr.
  void *search_{nullptr};
  void *search_tls_{nullptr};
  bool started_{false};
  std::vector<ServerCandidate> found_;
};

}  // namespace signalk
}  // namespace esphome

#endif
