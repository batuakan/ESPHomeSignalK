#pragma once
// The device id of a host (desktop) build. An ESP32 takes its id from the chip's MAC; a host build
// has no chip, so its id comes from the ESPHome node name and ESPHome's host MAC (`host:
// mac_address:`). Each build has its own id, the same one after every rebuild, and two copies of
// one build differ by setting another MAC in one of them. ESPHome keys a host build's preferences
// (the token among them) by the node name too. Pure, so it is unit tested (tests/cpp).

#include <cstdint>
#include <cstdio>
#include <string>

namespace esphome {
namespace signalk {

/// FNV-1a, 64 bit, continuing from `h`.
inline uint64_t fnv1a64(uint64_t h, const uint8_t *data, size_t len) {
  for (size_t i = 0; i < len; i++) {
    h ^= data[i];
    h *= 0x100000001b3ULL;
  }
  return h;
}

/// A UUID (version 8, RFC 9562: application defined) made from the node name and the host MAC.
inline std::string host_device_id(const std::string &name, const uint8_t mac[6]) {
  auto hash = [&](uint64_t h) {
    h = fnv1a64(h, reinterpret_cast<const uint8_t *>(name.data()), name.size());
    return fnv1a64(h, mac, 6);
  };
  const uint64_t hi = hash(0xcbf29ce484222325ULL);  // the FNV offset basis
  const uint64_t lo = hash(hi);                     // the same input again, from the first hash
  char out[37];
  std::snprintf(out, sizeof(out), "%08x-%04x-%04x-%04x-%012llx", static_cast<unsigned>(hi >> 32),
                static_cast<unsigned>((hi >> 16) & 0xFFFF),
                static_cast<unsigned>(0x8000 | (hi & 0x0FFF)),         // version 8
                static_cast<unsigned>(0x8000 | ((lo >> 48) & 0x3FFF)),  // variant 10
                static_cast<unsigned long long>(lo & 0xFFFFFFFFFFFFULL));
  return out;
}

}  // namespace signalk
}  // namespace esphome
