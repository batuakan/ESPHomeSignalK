// Host unit test for the saved server record (the server a device is bound to) and the checks used when
// choosing one (no ESPHome needed): tools/test_cpp.sh
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "../../components/signalk/server_discovery.h"

using namespace esphome::signalk;

static int failures = 0;
#define CHECK(cond) \
  do { \
    if (!(cond)) { \
      std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      failures++; \
    } \
  } while (0)

int main() {
  ServerCandidate s;
  s.hostname = "boatpc";
  s.address = "192.168.1.20";
  s.port = 3000;
  s.swname = "signalk-server";
  s.roles = "master, main";
  s.self = "urn:mrn:signalk:uuid:c0d79334-4e25-4245-8892-54e8ccc8021d";

  {  // round trip; the software name and roles are not kept
    SavedServer record;
    CHECK(to_saved_server(s, record));
    CHECK(std::string(record.hostname) == "boatpc" && std::string(record.address) == "192.168.1.20");
    CHECK(record.port == 3000);
    ServerCandidate back;
    CHECK(from_saved_server(record, back));
    CHECK(back.hostname == s.hostname && back.address == s.address && back.port == 3000 && back.self == s.self);
    CHECK(back.swname.empty() && back.roles.empty());
    CHECK(same_server(back, s));
    SavedServer again;
    CHECK(to_saved_server(back, again) && same_saved_server(record, again));
  }

  {  // what a reply found with mDNS may lack still round-trips: no address, no vessel id
    ServerCandidate n;
    n.hostname = "boatpc";
    n.port = 3443;
    SavedServer record;
    ServerCandidate back;
    CHECK(to_saved_server(n, record) && from_saved_server(record, back));
    CHECK(back.connect_host() == "boatpc.local" && back.port == 3443);
  }

  {  // too long to save: nothing is written rather than a cut record, and `out` is left as it was
    SavedServer record{};
    record.port = 1234;
    ServerCandidate big = s;
    big.self = std::string(sizeof(record.self), 'x');
    CHECK(!to_saved_server(big, record) && record.port == 1234);
    big = s;
    big.hostname = std::string(sizeof(record.hostname), 'h');
    CHECK(!to_saved_server(big, record));
    big = s;
    big.self = std::string(sizeof(record.self) - 1, 'x');  // just fits
    CHECK(to_saved_server(big, record));
  }

  {  // refused, and `out` left as it was
    SavedServer good;
    CHECK(to_saved_server(s, good));
    std::vector<SavedServer> bad;
    bad.push_back(SavedServer{});  // nothing saved (also how "no server" is stored)
    SavedServer r = good;
    r.port = 0;  // port 0
    bad.push_back(r);
    r = good;
    std::memset(r.hostname, 'a', sizeof(r.hostname));  // no terminating 0
    bad.push_back(r);
    r = good;
    std::memset(r.self, 'a', sizeof(r.self));
    bad.push_back(r);
    r = good;
    r.hostname[4] = '\t';  // control character
    bad.push_back(r);
    r = good;
    std::strcpy(r.address, "fe80::1");  // not an IPv4 address
    bad.push_back(r);
    r = good;
    r.hostname[0] = r.address[0] = '\0';  // nothing to connect to
    bad.push_back(r);
    for (size_t i = 0; i < bad.size(); i++) {
      ServerCandidate out = s;
      out.address = "10.0.0.1";
      if (from_saved_server(bad[i], out) || out.address != "10.0.0.1") {
        std::printf("FAIL: accepted bad record %u\n", static_cast<unsigned>(i));
        failures++;
      }
    }
  }

  {  // usable servers: what the list on the display offers
    ServerCandidate other = s;
    other.swname = "some-other-software";
    ServerCandidate unnamed = s;
    unnamed.swname.clear();
    ServerCandidate no_port = s;
    no_port.port = 0;
    ServerCandidate nowhere;
    nowhere.port = 3000;
    CHECK(usable_server(s));
    CHECK(usable_server(unnamed));
    CHECK(!usable_server(other));
    CHECK(!usable_server(no_port));
    CHECK(!usable_server(nowhere));
  }

  {  // a saved server is recognised after a restart of the server (new vessel id, new address)
    ServerCandidate saved;
    SavedServer record;
    CHECK(to_saved_server(s, record) && from_saved_server(record, saved));
    ServerCandidate restarted = s;
    restarted.self = "urn:mrn:signalk:uuid:00000000-0000-0000-0000-000000000000";
    restarted.address = "192.168.1.77";
    CHECK(same_server(saved, restarted));
    ServerCandidate neighbour = s;
    neighbour.hostname = "otherboat";
    neighbour.self = "urn:mrn:imo:mmsi:230000000";
    CHECK(!same_server(saved, neighbour));
  }

  std::printf("%d failures\n", failures);
  return failures == 0 ? 0 : 1;
}
