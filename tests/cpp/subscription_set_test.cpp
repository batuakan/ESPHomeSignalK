// Host unit test for SubscriptionSet (no ESPHome needed): tools/test_cpp.sh
#include <cstdio>
#include <string>

#include "../../components/signalk/subscription_set.h"

using esphome::signalk::SubscriptionRow;
using esphome::signalk::SubscriptionSet;
using Action = SubscriptionSet::Action;

static int failures = 0;
#define CHECK(cond)                                              \
  do {                                                           \
    if (!(cond)) {                                               \
      std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      failures++;                                                \
    }                                                            \
  } while (0)

static SubscriptionRow row(const char *path) {
  SubscriptionRow r;
  r.path = path;
  return r;
}

int main() {
  SubscriptionSet s;
  CHECK(s.pending() == Action::NONE);

  // New paths are sent incrementally.
  s.add(row("navigation.speedOverGround"));
  s.add(row("environment.wind.speedApparent"));
  CHECK(s.pending() == Action::SUBSCRIBE_ADDED);
  CHECK(s.added().size() == 2);
  s.mark_synced();
  CHECK(s.pending() == Action::NONE);

  s.add(row("environment.depth.belowTransducer"));
  CHECK(s.pending() == Action::SUBSCRIBE_ADDED);
  CHECK(s.added().size() == 1 && s.added()[0].path == "environment.depth.belowTransducer");
  s.mark_synced();

  // A path referenced twice stays until the last reference goes.
  s.add(row("navigation.speedOverGround"));
  CHECK(s.pending() == Action::NONE);
  s.remove("navigation.speedOverGround");
  CHECK(s.pending() == Action::NONE);
  CHECK(s.contains("navigation.speedOverGround"));

  // Any removal needs unsubscribe-all + full resubscribe.
  s.remove("navigation.speedOverGround");
  CHECK(!s.contains("navigation.speedOverGround"));
  CHECK(s.pending() == Action::RESUBSCRIBE_ALL);
  CHECK(s.all().size() == 2);
  s.mark_synced();
  CHECK(s.pending() == Action::NONE);

  // Removal plus addition in one batch still resubscribes everything.
  s.remove("environment.wind.speedApparent");
  s.add(row("navigation.headingTrue"));
  CHECK(s.pending() == Action::RESUBSCRIBE_ALL);
  s.mark_synced();

  // After a reconnect everything is "added" again (no unsubscribe on a fresh socket).
  s.mark_disconnected();
  CHECK(s.pending() == Action::SUBSCRIBE_ADDED);
  CHECK(s.added().size() == s.size());

  // Removing an unknown path is harmless; the first reference defines the row options.
  s.remove("no.such.path");
  SubscriptionRow slow = row("tanks.fuel.0.currentLevel");
  slow.period = 5000;
  s.add(slow);
  s.add(row("tanks.fuel.0.currentLevel"));
  for (const auto &r : s.all())
    if (r.path == "tanks.fuel.0.currentLevel")
      CHECK(r.period == 5000);

  std::printf("%s (%d failures)\n", failures == 0 ? "OK" : "FAILED", failures);
  return failures == 0 ? 0 : 1;
}
