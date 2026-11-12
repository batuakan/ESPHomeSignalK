// Host unit test for the host build's device id (host_device_id.h): tools/test_cpp.sh
#include <cstdio>
#include <string>

#include "../../components/signalk/host_device_id.h"

using namespace esphome::signalk;

static int failures = 0;
#define CHECK(cond) \
  do { \
    if (!(cond)) { \
      std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      failures++; \
    } \
  } while (0)

static bool is_uuid_v8(const std::string &id) {
  if (id.size() != 36)
    return false;
  for (size_t i = 0; i < id.size(); i++) {
    const bool dash = i == 8 || i == 13 || i == 18 || i == 23;
    const char c = id[i];
    if (dash ? c != '-' : !((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')))
      return false;
  }
  return id[14] == '8' && (id[19] == '8' || id[19] == '9' || id[19] == 'a' || id[19] == 'b');
}

int main() {
  const uint8_t mac[6] = {0x98, 0x35, 0x69, 0xab, 0xf6, 0x79};  // ESPHome's default host MAC
  const uint8_t other_mac[6] = {0x98, 0x35, 0x69, 0xab, 0xf6, 0x7a};

  const std::string sdl = host_device_id("esphelm-sdl", mac);
  // The same build: the same id, every time.
  CHECK(sdl == host_device_id("esphelm-sdl", mac));
  // A fixed value, so a change to the derivation (which would move every host build to a new
  // device) shows up here. ESPHelm's SDL e2e computes the same value (firmware/test/e2e_sdl.ts).
  CHECK(sdl == "30717c69-71c3-8b59-b52b-dea74971408d");
  CHECK(is_uuid_v8(sdl));
  // Other builds, and another MAC, get other ids.
  CHECK(sdl != host_device_id("esphelm-sdl-mono", mac));
  CHECK(sdl != host_device_id("esphelm-sdl-720", mac));
  CHECK(sdl != host_device_id("esphelm-sdl", other_mac));
  CHECK(is_uuid_v8(host_device_id("", mac)));

  if (failures == 0)
    std::printf("host_device_id: all passed\n");
  return failures == 0 ? 0 : 1;
}
