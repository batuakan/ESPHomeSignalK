#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
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

/// The server a device is bound to, kept in an ESPHome preference next to its token. The software name and
/// roles are not kept. Fields are 0-terminated; a record with port 0 means no server is bound.
struct SavedServer {
  char hostname[64];
  char address[16];  ///< IPv4 address as text
  char self[96];
  uint16_t port;
};

/// Copies `src` into the fixed field `dst`; false when it doesn't fit (with its terminating 0).
template<size_t N> inline bool copy_saved_field(char (&dst)[N], const std::string &src) {
  if (src.size() >= N)
    return false;
  std::memset(dst, 0, N);
  std::memcpy(dst, src.data(), src.size());
  return true;
}

/// Reads the fixed field `src` into `out`; false without a terminating 0 or with a control character.
template<size_t N> inline bool read_saved_field(const char (&src)[N], std::string &out) {
  const void *end = std::memchr(src, 0, N);
  if (end == nullptr)
    return false;
  std::string text(src, static_cast<const char *>(end) - src);
  for (char ch : text) {
    if (static_cast<unsigned char>(ch) < 0x20 || ch == 0x7f)
      return false;
  }
  out = std::move(text);
  return true;
}

/// Fills `out` for `s`. False, and nothing to save, when a field is too long: a cut record would name
/// another server.
inline bool to_saved_server(const ServerCandidate &s, SavedServer &out) {
  SavedServer r{};
  if (!copy_saved_field(r.hostname, s.hostname) || !copy_saved_field(r.address, s.address) ||
      !copy_saved_field(r.self, s.self))
    return false;
  r.port = s.port;
  out = r;
  return true;
}

/// Reads a saved record. Anything else than a record written by to_saved_server (a missing terminator, a
/// control character, an address that isn't IPv4 text, port 0, nothing to connect to) is refused and `out`
/// is left as it was: flash can hold a damaged record.
inline bool from_saved_server(const SavedServer &r, ServerCandidate &out) {
  ServerCandidate s;
  if (!read_saved_field(r.hostname, s.hostname) || !read_saved_field(r.address, s.address) ||
      !read_saved_field(r.self, s.self))
    return false;
  if (r.port == 0 || s.address.find_first_not_of("0123456789.") != std::string::npos)
    return false;
  if (s.hostname.empty() && s.address.empty())
    return false;
  s.port = r.port;
  out = s;
  return true;
}

/// True when both records name the same server in every saved field.
inline bool same_saved_server(const SavedServer &a, const SavedServer &b) {
  return a.port == b.port && std::strcmp(a.hostname, b.hostname) == 0 && std::strcmp(a.address, b.address) == 0 &&
         std::strcmp(a.self, b.self) == 0;
}

}  // namespace signalk
}  // namespace esphome
