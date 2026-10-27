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
  DiscoveryPoll poll(std::vector<ServerCandidate> &found);

 private:
  void *search_{nullptr};  // mdns_search_once_t *, kept opaque so this header needs no ESP-IDF includes
};

}  // namespace signalk
}  // namespace esphome

#endif
