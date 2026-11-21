// Host unit test for the pinned server certificate: the saved record and the trust store (no ESPHome needed):
// tools/test_cpp.sh
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include "../../components/signalk/tls_pin.h"

using namespace esphome::signalk;

static int failures = 0;
#define CHECK(cond) \
  do { \
    if (!(cond)) { \
      std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      failures++; \
    } \
  } while (0)

static Fingerprint fingerprint(uint8_t first) {
  Fingerprint fp{};
  for (size_t i = 0; i < fp.size(); i++)
    fp[i] = static_cast<uint8_t>(first + i);
  return fp;
}

int main() {
  const Fingerprint a = fingerprint(0x8b), b = fingerprint(0x10);

  {  // the text users compare with `openssl x509 -fingerprint -sha256`
    CHECK(format_fingerprint(a, 4) == "8B:8C:8D:8E");
    CHECK(format_fingerprint(a).size() == 32 * 3 - 1);
    CHECK(format_fingerprint(a, 0).empty());
  }

  {  // saved record: round trip, and only what to_saved_tls writes is read back
    bool tls = false, pinned = false;
    Fingerprint pin{};
    CHECK(from_saved_tls(to_saved_tls(true, true, a), tls, pinned, pin) && tls && pinned && pin == a);
    CHECK(from_saved_tls(to_saved_tls(true, false, a), tls, pinned, pin) && tls && !pinned && pin == Fingerprint{});
    CHECK(from_saved_tls(to_saved_tls(false, true, a), tls, pinned, pin) && !tls && !pinned);  // no pin without TLS

    SavedTls damaged = to_saved_tls(true, true, a);
    damaged.version = 0;  // an empty (erased) record
    tls = pinned = true;
    pin = b;
    CHECK(!from_saved_tls(damaged, tls, pinned, pin) && tls && pinned && pin == b);  // outputs untouched
    damaged = to_saved_tls(true, true, a);
    damaged.pinned = 2;
    CHECK(!from_saved_tls(damaged, tls, pinned, pin));
    damaged = to_saved_tls(true, true, a);
    damaged.tls = 0;  // a pin without TLS
    CHECK(!from_saved_tls(damaged, tls, pinned, pin));
    damaged = to_saved_tls(true, true, a);
    damaged.reserved = 1;
    CHECK(!from_saved_tls(damaged, tls, pinned, pin));
  }

  {  // first use: the first certificate is learned, then only it is accepted
    TlsTrust trust;
    Fingerprint out{};
    CHECK(!trust.pinned());
    CHECK(trust.check(a));
    CHECK(trust.pinned() && trust.pin() == a);
    CHECK(trust.take_learned(out) && out == a);
    CHECK(!trust.take_learned(out));  // reported once
    CHECK(trust.check(a));
    CHECK(!trust.take_refused(out));
    CHECK(!trust.check(b));
    CHECK(trust.pin() == a);  // a refusal never replaces the pin
    CHECK(trust.take_refused(out) && out == b);
    CHECK(!trust.take_refused(out));
  }

  {  // a saved pin is required from the start; clearing it pins anew
    TlsTrust trust;
    Fingerprint out{};
    trust.set(true, a);
    trust.set_ca_pem("-----BEGIN CERTIFICATE-----");
    CHECK(!trust.check(b));
    CHECK(trust.check(a));
    CHECK(!trust.take_learned(out));
    trust.clear();
    CHECK(!trust.pinned() && trust.ca_pem().empty());
    CHECK(!trust.take_refused(out));  // cleared with the pin
    CHECK(trust.check(b) && trust.pin() == b);
    trust.set(false, a);  // "not pinned" ignores the fingerprint
    CHECK(!trust.pinned() && trust.pin() == Fingerprint{});
  }

  {  // connections at once (the stream and a worker) on first use agree on one pin
    TlsTrust trust;
    std::vector<std::thread> threads;
    std::vector<int> accepted(8, 0);
    for (int i = 0; i < 8; i++)
      threads.emplace_back([&, i] { accepted[i] = trust.check(i % 2 == 0 ? a : b) ? 1 : 0; });
    for (auto &t : threads)
      t.join();
    int a_ok = 0, b_ok = 0;
    for (int i = 0; i < 8; i++)
      (i % 2 == 0 ? a_ok : b_ok) += accepted[i];
    CHECK((a_ok == 4 && b_ok == 0) || (a_ok == 0 && b_ok == 4));
  }

  if (failures == 0)
    std::printf("tls_pin_test: all passed\n");
  return failures == 0 ? 0 : 1;
}
