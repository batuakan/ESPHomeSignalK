#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace esphome {
namespace signalk {

/// mDNS service signalk-server advertises for its REST/HTTP API (and the WebSocket on the same port).
static const char *const SIGNALK_MDNS_SERVICE = "_signalk-http._tcp.local";

/// A SignalK server found with mDNS.
struct ServerCandidate {
  std::string hostname;  ///< mDNS host name (without ".local")
  std::string address;   ///< IPv4 address as text, empty when the reply did not include one
  uint16_t port{0};
  std::string swname;  ///< TXT "swname", e.g. signalk-server
  std::string roles;   ///< TXT "roles", e.g. "master, main"
  std::string self;    ///< TXT "self", the vessel's self id

  /// What to connect to. The address is preferred: host names like "Boat PC (2).local" are
  /// common and not resolvable by the device.
  std::string connect_host() const { return address.empty() ? hostname + ".local" : address; }
};

/// Result of one poll of an asynchronous server search.
enum class DiscoveryPoll {
  PENDING,  ///< still searching, poll again later
  DONE,     ///< search finished, the results (possibly none) were filled in
};

/// Chooses the server to use when several answered. Servers that identify as something else than
/// SignalK are skipped, the main server is preferred, and ties are broken by address and port so
/// that the same server is chosen on every boot. Returns nullptr when there is no usable server.
inline const ServerCandidate *pick_server(const std::vector<ServerCandidate> &found) {
  const ServerCandidate *best = nullptr;
  auto is_main = [](const ServerCandidate &c) { return c.roles.find("main") != std::string::npos; };
  for (const auto &c : found) {
    if (c.port == 0 || c.connect_host() == ".local")
      continue;
    if (!c.swname.empty() && c.swname.find("signalk") == std::string::npos)
      continue;
    if (best == nullptr) {
      best = &c;
      continue;
    }
    if (is_main(c) != is_main(*best)) {
      if (is_main(c))
        best = &c;
      continue;
    }
    if (std::make_pair(c.address, c.port) < std::make_pair(best->address, best->port))
      best = &c;
  }
  return best;
}

/// True when both are the same server: the same vessel id, or the same host name on the same port. The
/// address may differ, a server can come back on another address. Neither alone is dependable: the vessel
/// id changes on every start of a server that has no vessel UUID configured, and a host name is shared by
/// every server running on that machine, hence the port.
inline bool same_server(const ServerCandidate &a, const ServerCandidate &b) {
  if (!a.self.empty() && a.self == b.self)
    return true;
  return !a.hostname.empty() && a.hostname == b.hostname && a.port == b.port;
}

/// Adds `candidate` unless the same server (same address, or same host name when there is no address, and
/// same port) is already there; one server is often reported several times (an IPv4 and an IPv6 result,
/// several interfaces, several replies). An address-less duplicate is completed with the address.
inline void add_unique(std::vector<ServerCandidate> &found, const ServerCandidate &candidate) {
  for (auto &c : found) {
    if (c.port != candidate.port)
      continue;
    bool same = (!c.address.empty() && c.address == candidate.address) ||
                ((c.address.empty() || candidate.address.empty()) && c.hostname == candidate.hostname);
    if (!same)
      continue;
    if (c.address.empty())
      c.address = candidate.address;
    return;
  }
  found.push_back(candidate);
}

}  // namespace signalk
}  // namespace esphome
