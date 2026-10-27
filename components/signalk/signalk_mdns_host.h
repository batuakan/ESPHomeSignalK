#pragma once
#ifdef USE_HOST

#include <cstdint>
#include <vector>

#include "server_discovery.h"

namespace esphome {
namespace signalk {

/// Looks for SignalK servers from the host build, which has no mDNS client: sends one multicast query
/// and collects the unicast replies (see mdns_parse.h). Never blocks: poll() returns PENDING while the
/// search window is open.
class SignalKMdnsHost {
 public:
  ~SignalKMdnsHost();
  DiscoveryPoll poll(std::vector<ServerCandidate> &found);

 private:
  bool send_query();
  void close_socket();

  int fd_{-1};
  uint32_t started_at_{0};
  bool resent_{false};
  std::vector<ServerCandidate> found_;
};

}  // namespace signalk
}  // namespace esphome

#endif
