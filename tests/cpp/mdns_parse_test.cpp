// Host unit test for mDNS parsing and server selection (no ESPHome needed): tools/test_cpp.sh
#include <cstdio>
#include <string>
#include <vector>

#include "../../components/signalk/mdns_parse.h"

using namespace esphome::signalk;

static int failures = 0;
#define CHECK(cond) \
  do { \
    if (!(cond)) { \
      std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      failures++; \
    } \
  } while (0)

static const std::string SERVICE = "_signalk-http._tcp.local";

/// Builds mDNS replies shaped like the ones signalk-server's responder sends: the question echoed back,
/// one PTR answer, and SRV/TXT/A/AAAA as additional records, with names compressed against the question.
struct Reply {
  std::vector<uint8_t> b;
  size_t service_at{0};  // offset of the service name in the question
  size_t local_at{0};    // offset of the trailing "local" label
  uint16_t answers{0}, additional{0};

  Reply() : b(12, 0) {
    b[2] = 0x84;  // response, authoritative
    b[5] = 1;     // one question
    service_at = b.size();
    label("_signalk-http");
    label("_tcp");
    local_at = b.size();
    label("local");
    b.push_back(0);
    u16(12);
    u16(1);
  }

  void u16(uint16_t v) {
    b.push_back(v >> 8);
    b.push_back(v & 0xFF);
  }
  void label(const std::string &s) {
    b.push_back((uint8_t) s.size());
    b.insert(b.end(), s.begin(), s.end());
  }
  void pointer(size_t to) {
    b.push_back(0xC0 | (to >> 8));
    b.push_back(to & 0xFF);
  }
  /// Record header; the owner is `instance` followed by a pointer to the question's service name.
  size_t rr(uint16_t type, const std::string &instance, bool on_service) {
    if (on_service) {
      label(instance);
      pointer(service_at);
    } else {
      label(instance);
      pointer(local_at);  // "<host>.local"
    }
    u16(type);
    u16(1);
    u16(0);
    u16(10);  // ttl
    u16(0);   // rdlength, patched by end_rr
    return b.size() - 2;
  }
  void end_rr(size_t len_at) {
    size_t len = b.size() - len_at - 2;
    b[len_at] = len >> 8;
    b[len_at + 1] = len & 0xFF;
  }
  void srv(const std::string &instance, const std::string &host, uint16_t port) {
    size_t at = rr(33, instance, true);
    u16(0);
    u16(0);
    u16(port);
    label(host);
    pointer(local_at);
    end_rr(at);
    additional++;
  }
  void txt(const std::string &instance, const std::vector<std::string> &items) {
    size_t at = rr(16, instance, true);
    for (const auto &i : items)
      label(i);
    end_rr(at);
    additional++;
  }
  void a(const std::string &host, uint8_t o1, uint8_t o2, uint8_t o3, uint8_t o4) {
    size_t at = rr(1, host, false);
    b.push_back(o1);
    b.push_back(o2);
    b.push_back(o3);
    b.push_back(o4);
    end_rr(at);
    additional++;
  }
  void aaaa(const std::string &host) {
    size_t at = rr(28, host, false);
    for (int i = 0; i < 16; i++)
      b.push_back(i == 0 ? 0xFE : 0);
    end_rr(at);
    additional++;
  }
  const std::vector<uint8_t> &done() {
    b[7] = 0;  // no PTR answer is written, the parser works from the SRV records
    b[10] = additional >> 8;
    b[11] = additional & 0xFF;
    return b;
  }
};

static std::vector<uint8_t> typical(const std::string &instance, const std::string &host, uint16_t port,
                                    const std::string &roles = "master, main",
                                    const std::string &swname = "signalk-server", const char *ip = "192.168.0.50") {
  Reply r;
  r.srv(instance, host, port);
  std::vector<std::string> txt = {"txtvers=1", "swname=" + swname, "swvers=2.30.0", "roles=" + roles,
                                  "self=urn:mrn:signalk:uuid:abc"};
  r.txt(instance, txt);
  unsigned o[4];
  std::sscanf(ip, "%u.%u.%u.%u", &o[0], &o[1], &o[2], &o[3]);
  r.a(host, o[0], o[1], o[2], o[3]);
  r.aaaa(host);
  return r.done();
}

static std::vector<ServerCandidate> parse(const std::vector<uint8_t> &p) {
  std::vector<ServerCandidate> out;
  mdns_parse_response(p.data(), p.size(), SERVICE, out);
  return out;
}

int main() {
  // A typical reply: instance "Boat", host "Boat PC (2).local" (not resolvable by name, the address is used).
  {
    auto found = parse(typical("Boat", "Boat PC (2)", 3000));
    CHECK(found.size() == 1);
    CHECK(found[0].port == 3000);
    CHECK(found[0].address == "192.168.0.50");
    CHECK(found[0].hostname == "Boat PC (2)");
    CHECK(found[0].swname == "signalk-server");
    CHECK(found[0].roles == "master, main");
    CHECK(found[0].self == "urn:mrn:signalk:uuid:abc");
    CHECK(found[0].connect_host() == "192.168.0.50");
  }

  // No A record in the reply: fall back to the host name.
  {
    Reply r;
    r.srv("Boat", "boatpc", 3000);
    r.txt("Boat", {"swname=signalk-server"});
    auto found = parse(r.done());
    CHECK(found.size() == 1);
    CHECK(found[0].address.empty());
    CHECK(found[0].connect_host() == "boatpc.local");
  }

  // No TXT record: still usable, the server is just not identified.
  {
    Reply r;
    r.srv("Boat", "boatpc", 3000);
    r.a("boatpc", 10, 0, 0, 7);
    auto found = parse(r.done());
    CHECK(found.size() == 1);
    CHECK(found[0].swname.empty());
    CHECK(found[0].address == "10.0.0.7");
  }

  // Replies of two servers, and the same server twice: duplicates are dropped.
  {
    std::vector<ServerCandidate> found;
    auto a = typical("Boat", "boatpc", 3000, "master, main", "signalk-server", "192.168.0.50");
    auto b = typical("Spare", "sparepc", 3000, "master, aux", "signalk-server", "192.168.0.60");
    mdns_parse_response(a.data(), a.size(), SERVICE, found);
    mdns_parse_response(b.data(), b.size(), SERVICE, found);
    mdns_parse_response(a.data(), a.size(), SERVICE, found);
    CHECK(found.size() == 2);
  }

  // One server reported twice, once without an address (e.g. an IPv6 only result): merged, address kept.
  {
    std::vector<ServerCandidate> found;
    ServerCandidate no_address, with_address;
    no_address.hostname = with_address.hostname = "boatpc";
    no_address.port = with_address.port = 3000;
    with_address.address = "192.168.0.50";
    add_unique(found, no_address);
    add_unique(found, with_address);
    CHECK(found.size() == 1);
    CHECK(found[0].address == "192.168.0.50");
    add_unique(found, no_address);
    CHECK(found.size() == 1);
    CHECK(found[0].address == "192.168.0.50");
    ServerCandidate other = with_address;
    other.hostname = "other";
    other.address = "192.168.0.51";
    add_unique(found, other);
    CHECK(found.size() == 2);
  }

  // Recognising a server again: the same vessel id, or the same host name on the same port.
  {
    ServerCandidate was, now;
    was.self = now.self = "urn:mrn:signalk:uuid:abc";
    was.hostname = "boatpc";
    was.port = now.port = 3000;
    now.hostname = "renamed";
    now.address = "10.0.0.9";
    CHECK(same_server(was, now));  // same vessel id, other name and address
    now.self = "urn:mrn:signalk:uuid:other";
    now.hostname = "boatpc";
    CHECK(same_server(was, now));  // restarted without a vessel UUID configured: new id, same machine and port
    now.port = 3100;
    CHECK(!same_server(was, now));  // another server on the same machine
    now.port = 3000;
    now.hostname = "other";
    CHECK(!same_server(was, now));  // another machine and another vessel
    was.self.clear();
    now.self.clear();
    CHECK(!same_server(was, now));
    ServerCandidate nameless_a, nameless_b;
    CHECK(!same_server(nameless_a, nameless_b));  // nothing to recognise it by
  }

  // Two servers on the same machine are different servers (different ports).
  {
    std::vector<ServerCandidate> found;
    auto a = typical("One", "pc", 3000);
    auto b = typical("Two", "pc", 3100);
    mdns_parse_response(a.data(), a.size(), SERVICE, found);
    mdns_parse_response(b.data(), b.size(), SERVICE, found);
    CHECK(found.size() == 2);
  }

  // A query from another device is not a reply.
  {
    auto q = mdns_build_query(SERVICE);
    std::vector<ServerCandidate> found;
    CHECK(!mdns_parse_response(q.data(), q.size(), SERVICE, found));
    CHECK(found.empty());
  }

  // Our own query: header, one question with the service labels, type PTR, class IN.
  {
    auto q = mdns_build_query(SERVICE);
    std::vector<uint8_t> expected = {0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 13};
    for (char c : std::string("_signalk-http"))
      expected.push_back(c);
    expected.push_back(4);
    for (char c : std::string("_tcp"))
      expected.push_back(c);
    expected.push_back(5);
    for (char c : std::string("local"))
      expected.push_back(c);
    for (uint8_t c : {0, 0, 12, 0, 1})
      expected.push_back(c);
    CHECK(q == expected);
  }

  // Service of another kind is ignored.
  {
    Reply r;
    r.srv("Printer", "printer", 631);
    std::vector<ServerCandidate> found;
    auto p = r.done();
    mdns_parse_response(p.data(), p.size(), "_ipp._tcp.local", found);
    CHECK(found.empty());
  }

  // Hostile input must not crash or hang: every truncation of a valid reply, and a pointer loop.
  {
    auto p = typical("Boat", "boatpc", 3000);
    for (size_t n = 0; n < p.size(); n++) {
      std::vector<ServerCandidate> found;
      mdns_parse_response(p.data(), n, SERVICE, found);
    }
    // the question name points at itself
    std::vector<uint8_t> loop(p.begin(), p.begin() + 12);
    loop.push_back(0xC0);
    loop.push_back(12);
    loop.push_back(0);
    loop.push_back(12);
    loop.push_back(0);
    loop.push_back(1);
    std::vector<ServerCandidate> found;
    CHECK(!mdns_parse_response(loop.data(), loop.size(), SERVICE, found));
    // a record count far larger than the data
    auto big = p;
    big[10] = 0xFF;
    big[11] = 0xFF;
    found.clear();
    CHECK(!mdns_parse_response(big.data(), big.size(), SERVICE, found));
    // flipping each byte must also stay safe
    for (size_t i = 0; i < p.size(); i++) {
      auto m = p;
      m[i] ^= 0xFF;
      found.clear();
      mdns_parse_response(m.data(), m.size(), SERVICE, found);
    }
  }

  // Choosing among servers.
  {
    ServerCandidate main_server;
    main_server.address = "192.168.0.60";
    main_server.port = 3000;
    main_server.roles = "master, main";
    main_server.swname = "signalk-server";
    ServerCandidate aux = main_server;
    aux.address = "192.168.0.10";
    aux.roles = "slave, aux";
    ServerCandidate other = main_server;
    other.address = "192.168.0.5";
    other.swname = "some-web-server";
    ServerCandidate unknown_sw = main_server;  // a reply without TXT is accepted
    unknown_sw.swname.clear();
    unknown_sw.roles.clear();
    unknown_sw.address = "192.168.0.99";

    CHECK(pick_server({}) == nullptr);
    CHECK(pick_server({other}) == nullptr);                             // not a SignalK server
    CHECK(pick_server({aux, main_server})->address == "192.168.0.60");  // the main server wins over a lower address
    CHECK(pick_server({other, aux})->address == "192.168.0.10");
    CHECK(pick_server({unknown_sw})->address == "192.168.0.99");

    // ties are broken by address then port, whatever the order of arrival
    ServerCandidate a = main_server, b = main_server;
    a.address = "192.168.0.20";
    b.address = "192.168.0.30";
    CHECK(pick_server({b, a})->address == "192.168.0.20");
    CHECK(pick_server({a, b})->address == "192.168.0.20");
    ServerCandidate p3000 = a, p3100 = a;
    p3100.port = 3100;
    CHECK(pick_server({p3100, p3000})->port == 3000);

    ServerCandidate no_port = main_server;
    no_port.port = 0;
    CHECK(pick_server({no_port}) == nullptr);
    ServerCandidate nothing;
    nothing.port = 3000;
    CHECK(pick_server({nothing}) == nullptr);  // neither address nor host name
  }

  if (failures == 0)
    std::printf("mdns_parse_test: all passed\n");
  return failures == 0 ? 0 : 1;
}
