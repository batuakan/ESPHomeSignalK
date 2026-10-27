#pragma once

// Minimal mDNS (DNS-SD) packet building and parsing, used where the platform has no mDNS client
// (the host build). Only what is needed to find a SignalK server: one PTR question, and reading the
// SRV, TXT and A records of the answer. Replies come from the network and are not trusted: every read
// is bounds checked and name compression is limited.

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

#include "server_discovery.h"

namespace esphome {
namespace signalk {

namespace mdns_detail {

static const uint16_t TYPE_A = 1;
static const uint16_t TYPE_TXT = 16;
static const uint16_t TYPE_SRV = 33;
static const uint16_t TYPE_PTR = 12;

inline std::string lower(std::string s) {
  for (auto &c : s)
    c = (char) std::tolower((unsigned char) c);
  return s;
}

inline bool ends_with(const std::string &s, const std::string &suffix) {
  return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

struct Reader {
  const uint8_t *data;
  size_t len;

  bool u16(size_t at, uint16_t &v) const {
    if (at + 2 > len)
      return false;
    v = (uint16_t) ((data[at] << 8) | data[at + 1]);
    return true;
  }

  /// Reads a (possibly compressed) domain name starting at `at` into dotted text. `next` is the offset
  /// after the name in the original position (not following pointers).
  bool name(size_t at, std::string &out, size_t &next) const {
    out.clear();
    bool jumped = false;
    int jumps = 0;
    size_t pos = at;
    while (true) {
      if (pos >= len)
        return false;
      uint8_t l = data[pos];
      if (l == 0) {
        if (!jumped)
          next = pos + 1;
        return true;
      }
      if ((l & 0xC0) == 0xC0) {
        if (pos + 2 > len || ++jumps > 16)
          return false;
        size_t target = (size_t) (((l & 0x3F) << 8) | data[pos + 1]);
        if (!jumped)
          next = pos + 2;
        jumped = true;
        pos = target;
        continue;
      }
      if ((l & 0xC0) != 0 || pos + 1 + l > len || out.size() + l + 1 > 255)
        return false;
      if (!out.empty())
        out += '.';
      out.append(reinterpret_cast<const char *>(data + pos + 1), l);
      pos += 1 + l;
    }
  }
};

}  // namespace mdns_detail

/// Builds a one-question mDNS query for the PTR records of `service` (e.g. "_signalk-http._tcp.local").
inline std::vector<uint8_t> mdns_build_query(const std::string &service) {
  std::vector<uint8_t> q(12, 0);
  q[5] = 1;  // one question
  size_t start = 0;
  while (start <= service.size()) {
    size_t dot = service.find('.', start);
    if (dot == std::string::npos)
      dot = service.size();
    size_t n = std::min<size_t>(dot - start, 63);
    if (n > 0) {
      q.push_back((uint8_t) n);
      q.insert(q.end(), service.begin() + start, service.begin() + start + n);
    }
    start = dot + 1;
  }
  q.push_back(0);
  q.push_back(0);
  q.push_back(mdns_detail::TYPE_PTR);  // type PTR
  q.push_back(0);
  q.push_back(1);  // class IN
  return q;
}

/// Reads an mDNS reply and adds every server of `service` it describes (an SRV record with the port and
/// host, plus the TXT record of the instance and the A record of the host when the reply carries them).
/// Returns false when the packet is not a well formed reply.
inline bool mdns_parse_response(const uint8_t *data, size_t len, const std::string &service,
                                std::vector<ServerCandidate> &out) {
  using namespace mdns_detail;
  mdns_detail::Reader r{data, len};
  uint16_t flags, qd, an, ns, ar;
  if (len < 12 || !r.u16(2, flags) || !r.u16(4, qd) || !r.u16(6, an) || !r.u16(8, ns) || !r.u16(10, ar))
    return false;
  if (!(flags & 0x8000))
    return false;  // a query from someone else

  size_t pos = 12;
  for (uint16_t i = 0; i < qd; i++) {
    std::string n;
    if (!r.name(pos, n, pos) || pos + 4 > len)
      return false;
    pos += 4;
  }

  struct Srv {
    std::string target;
    uint16_t port;
    std::string instance;
  };
  std::vector<Srv> servers;
  std::map<std::string, std::vector<std::string>> txt;  // instance (lower case) -> "key=value"
  std::map<std::string, std::string> addresses;         // host (lower case) -> first IPv4

  const std::string suffix = "." + lower(service);
  uint32_t records = (uint32_t) an + ns + ar;
  for (uint32_t i = 0; i < records; i++) {
    std::string owner;
    uint16_t type, rdlen;
    if (!r.name(pos, owner, pos) || !r.u16(pos, type) || !r.u16(pos + 8, rdlen))
      return false;
    size_t rdata = pos + 10;
    if (rdata + rdlen > len)
      return false;
    pos = rdata + rdlen;

    std::string key = lower(owner);
    if (type == TYPE_SRV && rdlen >= 7 && ends_with(key, suffix)) {
      uint16_t port;
      std::string target;
      size_t unused;
      if (r.u16(rdata + 4, port) && r.name(rdata + 6, target, unused))
        servers.push_back({target, port, key});
    } else if (type == TYPE_TXT) {
      size_t at = rdata;
      while (at < rdata + rdlen) {
        size_t l = data[at];
        if (at + 1 + l > rdata + rdlen)
          break;
        txt[key].push_back(std::string(reinterpret_cast<const char *>(data + at + 1), l));
        at += 1 + l;
      }
    } else if (type == TYPE_A && rdlen == 4 && addresses.find(key) == addresses.end()) {
      char buf[16];
      snprintf(buf, sizeof(buf), "%u.%u.%u.%u", data[rdata], data[rdata + 1], data[rdata + 2], data[rdata + 3]);
      addresses[key] = buf;
    }
  }

  for (const auto &s : servers) {
    ServerCandidate c;
    c.port = s.port;
    c.hostname = ends_with(lower(s.target), ".local") ? s.target.substr(0, s.target.size() - 6) : s.target;
    auto a = addresses.find(lower(s.target));
    if (a != addresses.end())
      c.address = a->second;
    auto t = txt.find(s.instance);
    if (t != txt.end()) {
      for (const auto &kv : t->second) {
        size_t eq = kv.find('=');
        if (eq == std::string::npos)
          continue;
        std::string k = lower(kv.substr(0, eq)), v = kv.substr(eq + 1);
        if (k == "swname")
          c.swname = v;
        else if (k == "roles")
          c.roles = v;
        else if (k == "self")
          c.self = v;
      }
    }
    add_unique(out, c);
  }
  return true;
}

}  // namespace signalk
}  // namespace esphome
