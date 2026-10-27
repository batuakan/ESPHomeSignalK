// Host unit test for the server-wins metadata merge (needs ArduinoJson): tools/test_cpp.sh
#include <cstdio>
#include <string>

#include <ArduinoJson.h>

#include "../../components/signalk/signalk_metadata.h"

static int failures = 0;
#define CHECK(cond) \
  do { \
    if (!(cond)) { \
      std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      failures++; \
    } \
  } while (0)

/// Diff of `ours` against the `server` JSON, serialized.
static std::string diff(const char *ours, const char *server) {
  SignalKMetadata meta;
  JsonDocument ours_doc;
  deserializeJson(ours_doc, ours);
  for (JsonPair kv : ours_doc.as<JsonObject>())
    meta.obj()[kv.key()] = kv.value();
  JsonDocument server_doc;
  deserializeJson(server_doc, server);
  JsonDocument out_doc;
  JsonObject out = out_doc.to<JsonObject>();
  meta.missing_from(server_doc.as<JsonVariantConst>(), out);
  std::string s;
  serializeJson(out_doc, s);
  return s;
}

int main() {
  // The server has nothing for the path (HTTP 404): everything is pushed.
  CHECK(diff(R"({"a":1,"b":"x"})", "null") == R"({"a":1,"b":"x"})");
  CHECK(diff(R"({"a":1,"b":"x"})", "{}") == R"({"a":1,"b":"x"})");

  // The server has every key: nothing to push, even when the values differ (server wins).
  CHECK(diff(R"({"a":1,"b":"x"})", R"({"a":1,"b":"x"})") == "{}");
  CHECK(diff(R"({"displayName":"ESP"})", R"({"displayName":"Server"})") == "{}");

  // Partial overlap: only the keys the server lacks.
  CHECK(diff(R"({"a":1,"b":"x"})", R"({"a":99})") == R"({"b":"x"})");

  // Falsy server values are still values.
  CHECK(diff(R"({"supportsPut":true,"timeout":5})", R"({"supportsPut":false,"timeout":0})") == "{}");

  // An empty string is the server's placeholder (it adds "description": "" to every path), so it counts as
  // missing, but an empty string of ours is not worth sending.
  CHECK(diff(R"({"description":"d"})", R"({"description":""})") == R"({"description":"d"})");
  CHECK(diff(R"({"description":""})", R"({"description":""})") == "{}");
  CHECK(diff(R"({"description":""})", "{}") == "{}");
  CHECK(diff(R"({"description":"d"})", R"({"description":"set on server"})") == "{}");

  // A null on the server counts as missing.
  CHECK(diff(R"({"a":1})", R"({"a":null})") == R"({"a":1})");

  // Nested objects merge key by key.
  CHECK(diff(R"({"displayScale":{"lower":0,"upper":75,"type":"linear"}})",
             R"({"displayScale":{"lower":10,"type":"log"}})") == R"({"displayScale":{"upper":75}})");
  CHECK(diff(R"({"displayScale":{"lower":0,"upper":75}})", R"({"displayScale":{"lower":1,"upper":2}})") == "{}");

  // Arrays are atomic: any server value wins, no per-element merge.
  CHECK(diff(R"({"zones":[{"upper":4,"state":"alarm"}]})", R"({"zones":[{"upper":9,"state":"warn"}]})") == "{}");
  CHECK(diff(R"({"zones":[{"upper":4,"state":"alarm"}]})", R"({"description":"x"})") ==
        R"({"zones":[{"upper":4,"state":"alarm"}]})");

  // Object on our side, scalar on the server: the server value wins.
  CHECK(diff(R"({"displayScale":{"lower":0}})", R"({"displayScale":"none"})") == "{}");

  // empty()
  SignalKMetadata meta;
  CHECK(meta.empty());
  meta.set("a", 1.0);
  CHECK(!meta.empty());

  if (failures == 0)
    std::printf("metadata_test: all passed\n");
  return failures == 0 ? 0 : 1;
}
