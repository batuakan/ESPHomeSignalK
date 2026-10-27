#ifdef USE_HOST

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "esphome/core/log.h"
#include "mdns_parse.h"
#include "signalk_mdns_host.h"
#include "utils.h"

namespace esphome {
namespace signalk {

static const char *const TAG = "signalk";

// The search window; the query is sent again halfway in case the first one was lost.
static const uint32_t SEARCH_WINDOW_MS = 2000;
static const uint16_t MDNS_PORT = 5353;
static const char *const MDNS_GROUP = "224.0.0.251";

SignalKMdnsHost::~SignalKMdnsHost() { close_socket(); }

void SignalKMdnsHost::close_socket() {
  if (fd_ >= 0) {
    ::close(fd_);
    fd_ = -1;
  }
}

bool SignalKMdnsHost::send_query() {
  std::vector<uint8_t> query = mdns_build_query(SIGNALK_MDNS_SERVICE);
  sockaddr_in group{};
  group.sin_family = AF_INET;
  group.sin_port = htons(MDNS_PORT);
  inet_pton(AF_INET, MDNS_GROUP, &group.sin_addr);
  ssize_t sent = sendto(fd_, query.data(), query.size(), 0, reinterpret_cast<sockaddr *>(&group), sizeof(group));
  return sent == (ssize_t) query.size();
}

DiscoveryPoll SignalKMdnsHost::poll(std::vector<ServerCandidate> &found) {
  found.clear();
  uint32_t now = sk_millis();

  if (fd_ < 0) {
    fd_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd_ < 0) {
      ESP_LOGW(TAG, "Could not open a socket for the mDNS search");
      return DiscoveryPoll::DONE;
    }
    fcntl(fd_, F_SETFL, fcntl(fd_, F_GETFL, 0) | O_NONBLOCK);
    unsigned char ttl = 255;
    setsockopt(fd_, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl));
    // An ephemeral source port (not 5353) makes responders answer us directly instead of multicasting.
    sockaddr_in any{};
    any.sin_family = AF_INET;
    any.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(fd_, reinterpret_cast<sockaddr *>(&any), sizeof(any)) != 0 || !send_query()) {
      ESP_LOGW(TAG, "Could not send the mDNS query (no multicast route?)");
      close_socket();
      return DiscoveryPoll::DONE;
    }
    found_.clear();
    started_at_ = now;
    resent_ = false;
    return DiscoveryPoll::PENDING;
  }

  uint8_t buffer[2048];
  ssize_t n;
  while ((n = recv(fd_, buffer, sizeof(buffer), 0)) > 0)
    mdns_parse_response(buffer, (size_t) n, SIGNALK_MDNS_SERVICE, found_);

  if (!resent_ && now - started_at_ >= SEARCH_WINDOW_MS / 2) {
    send_query();
    resent_ = true;
  }
  if (now - started_at_ < SEARCH_WINDOW_MS)
    return DiscoveryPoll::PENDING;

  close_socket();
  found = std::move(found_);
  found_.clear();
  return DiscoveryPoll::DONE;
}

}  // namespace signalk
}  // namespace esphome

#endif
