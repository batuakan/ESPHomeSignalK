// Host unit test for the saved server record (the server a device is bound to) and the checks used when
// choosing one (no ESPHome needed): tools/test_cpp.sh
#include <cstdio>
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
    const std::string record = encode_saved_server(s);
    CHECK(record == "v1\nboatpc\n192.168.1.20\n3000\nurn:mrn:signalk:uuid:c0d79334-4e25-4245-8892-54e8ccc8021d");
    ServerCandidate back;
    CHECK(decode_saved_server(record, back));
    CHECK(back.hostname == s.hostname && back.address == s.address && back.port == 3000 && back.self == s.self);
    CHECK(back.swname.empty() && back.roles.empty());
    CHECK(same_server(back, s));
  }

  {  // what a reply found with mDNS may lack still round-trips: no address, no vessel id
    ServerCandidate n;
    n.hostname = "boatpc";
    n.port = 3443;
    ServerCandidate back;
    CHECK(decode_saved_server(encode_saved_server(n), back));
    CHECK(back.connect_host() == "boatpc.local" && back.port == 3443);
  }

  {  // too long to save: nothing is written rather than a cut record
    ServerCandidate big = s;
    big.self = std::string(SAVED_SERVER_MAX, 'x');
    CHECK(encode_saved_server(big).empty());
  }

  {  // refused, and `out` left as it was
    const char *bad[] = {
        "",                                     // nothing saved (also how "no server" is stored)
        "v2\nboatpc\n192.168.1.20\n3000\nself",  // another version
        "v1\nboatpc\n192.168.1.20\n3000",       // a field missing
        "v1\nboatpc\n192.168.1.20\n3000\nself\nx",
        "v1\nboatpc\n192.168.1.20\n0\nself",       // port 0
        "v1\nboatpc\n192.168.1.20\n65536\nself",   // port out of range
        "v1\nboatpc\n192.168.1.20\n30a0\nself",
        "v1\nboatpc\n192.168.1.20\n\nself",
        "v1\nboatpc\n192.168.1.20\n123456\nself",
        "v1\nboat\tpc\n192.168.1.20\n3000\nself",  // control character
        "v1\nboatpc\nfe80::1\n3000\nself",         // not an IPv4 address
        "v1\nboatpc\n192.168.100.200.1\n3000\nself",
        "v1\n\n\n3000\nself",  // nothing to connect to
    };
    for (const char *b : bad) {
      ServerCandidate out = s;
      if (decode_saved_server(b, out) || out.address != s.address) {
        std::printf("FAIL: accepted \"%s\"\n", b);
        failures++;
      }
    }
    std::string too_long = "v1\nboatpc\n192.168.1.20\n3000\n" + std::string(SAVED_SERVER_MAX, 'x');
    ServerCandidate out;
    CHECK(!decode_saved_server(too_long, out));
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
    CHECK(decode_saved_server(encode_saved_server(s), saved));
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
