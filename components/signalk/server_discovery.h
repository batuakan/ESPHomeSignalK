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

/// True when `c` can be connected to and does not identify as something else than SignalK.
inline bool usable_server(const ServerCandidate &c) {
  if (c.port == 0 || c.connect_host() == ".local")
    return false;
  return c.swname.empty() || c.swname.find("signalk") != std::string::npos;
}

/// Chooses the server to use when several answered. Servers that identify as something else than
/// SignalK are skipped, the main server is preferred, and ties are broken by address and port so
/// that the same server is chosen on every boot. Returns nullptr when there is no usable server.
inline const ServerCandidate *pick_server(const std::vector<ServerCandidate> &found) {
  const ServerCandidate *best = nullptr;
  auto is_main = [](const ServerCandidate &c) { return c.roles.find("main") != std::string::npos; };
  for (const auto &c : found) {
    if (!usable_server(c))
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

/// Longest saved server record (encode_saved_server), without the terminating 0.
static const size_t SAVED_SERVER_MAX = 200;

/// The server a device is bound to, as saved in flash next to its token: a version line, then the host
/// name, address, port and vessel id, one per line. The software name and roles are not kept.
inline std::string encode_saved_server(const ServerCandidate &s) {
  std::string out = "v1\n" + s.hostname + "\n" + s.address + "\n" + std::to_string(s.port) + "\n" + s.self;
  return out.size() <= SAVED_SERVER_MAX ? out : std::string();
}

/// Reads a record written by encode_saved_server. Anything else (another version, a missing or extra
/// field, a control character, a port out of range, nothing to connect to) is refused and `out` is left
/// as it was: flash can hold a record from other firmware, or a damaged one.
inline bool decode_saved_server(const std::string &text, ServerCandidate &out) {
  if (text.size() > SAVED_SERVER_MAX)
    return false;
  std::vector<std::string> fields;
  size_t start = 0;
  while (true) {
    size_t end = text.find('\n', start);
    fields.push_back(text.substr(start, end == std::string::npos ? std::string::npos : end - start));
    if (end == std::string::npos)
      break;
    start = end + 1;
  }
  if (fields.size() != 5 || fields[0] != "v1")
    return false;
  for (const auto &f : fields) {
    for (char ch : f) {
      if (static_cast<unsigned char>(ch) < 0x20 || ch == 0x7f)
        return false;
    }
  }
  const std::string &port = fields[3];
  if (port.empty() || port.size() > 5 || port.find_first_not_of("0123456789") != std::string::npos)
    return false;
  unsigned long port_value = std::stoul(port);
  if (port_value == 0 || port_value > 65535)
    return false;
  if (fields[2].find_first_not_of("0123456789.") != std::string::npos || fields[2].size() > 15)
    return false;
  ServerCandidate s;
  s.hostname = fields[1];
  s.address = fields[2];
  s.port = static_cast<uint16_t>(port_value);
  s.self = fields[4];
  if (s.hostname.empty() && s.address.empty())
    return false;
  out = s;
  return true;
}

}  // namespace signalk
}  // namespace esphome
