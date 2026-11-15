// Host unit test for forwarded PUT requests (needs ArduinoJson): tools/test_cpp.sh
// The messages are what signalk-server 2.37 writes to a WebSocket client that publishes the path.
#include <cstdio>
#include <string>

#include <ArduinoJson.h>

#include "../../components/signalk/put_handling.h"

using namespace esphome::signalk;

static int failures = 0;
#define CHECK(cond) \
  do { \
    if (!(cond)) { \
      std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      failures++; \
    } \
  } while (0)

static bool parse(const char *json, JsonDocument &doc, ForwardedPut &out) {
  if (deserializeJson(doc, json))
    return false;
  return parse_forwarded_put(doc.as<JsonVariantConst>(), out);
}

int main() {
  JsonDocument doc;
  ForwardedPut put;

  // What the server sends (ws.ts handlePut): an array with one entry.
  CHECK(parse("{\"requestId\":\"abc-1\",\"context\":\"vessels.self\","
              "\"put\":[{\"path\":\"steering.autopilot.ac10.steps\",\"value\":-10}]}",
              doc, put));
  CHECK(put.request_id == "abc-1");
  CHECK(put.context == "vessels.self");
  CHECK(put.items.size() == 1);
  CHECK(put.items[0].path == "steering.autopilot.ac10.steps");
  double n = 0;
  CHECK(put_value_number(put.items[0].value, n) && n == -10);

  // The object form, as a client sends it; no context means self.
  CHECK(parse("{\"requestId\":\"r2\",\"put\":{\"path\":\"a.b\",\"value\":true}}", doc, put));
  CHECK(put.context == "vessels.self");
  CHECK(put.items.size() == 1);
  bool b = false;
  CHECK(put_value_bool(put.items[0].value, b) && b);

  // Not forwarded PUTs: a delta, the server's reply to our own PUT, a put without requestId.
  CHECK(!parse("{\"updates\":[{\"values\":[{\"path\":\"a.b\",\"value\":1}]}]}", doc, put));
  CHECK(!parse("{\"requestId\":\"x\",\"state\":\"COMPLETED\",\"statusCode\":200}", doc, put));
  CHECK(!parse("{\"put\":{\"path\":\"a.b\",\"value\":1}}", doc, put));

  // Entries without a path are skipped.
  CHECK(parse("{\"requestId\":\"r3\",\"put\":[{\"value\":1},{\"path\":\"a.c\",\"value\":\"x\"}]}", doc, put));
  CHECK(put.items.size() == 1 && put.items[0].path == "a.c");

  // Value conversions.
  CHECK(parse("{\"requestId\":\"r4\",\"put\":[{\"path\":\"p\",\"value\":\"off\"},{\"path\":\"q\",\"value\":2.5},"
              "{\"path\":\"r\",\"value\":{\"cmd\":\"x\"}},{\"path\":\"s\"},{\"path\":\"t\",\"value\":\"maybe\"}]}",
              doc, put));
  CHECK(put.items.size() == 5);
  CHECK(put_value_bool(put.items[0].value, b) && !b);
  CHECK(!put_value_number(put.items[0].value, n));
  CHECK(put_value_number(put.items[1].value, n) && n == 2.5);
  CHECK(put_value_bool(put.items[1].value, b) && b);
  CHECK(put_value_text(put.items[0].value) == "off");
  CHECK(put_value_text(put.items[2].value) == "{\"cmd\":\"x\"}");
  CHECK(put_value_text(put.items[3].value) == "null");
  CHECK(!put_value_bool(put.items[4].value, b));

  // Replies.
  CHECK(put_reply_json("abc-1", 200, "") == "{\"requestId\":\"abc-1\",\"state\":\"COMPLETED\",\"statusCode\":200}");
  CHECK(put_reply_json("r", 400, "not engaged") ==
        "{\"requestId\":\"r\",\"state\":\"COMPLETED\",\"statusCode\":400,\"message\":\"not engaged\"}");
  CHECK(put_reply_json("r", 202, "") == "{\"requestId\":\"r\",\"state\":\"PENDING\",\"statusCode\":202}");

  // One reply per request, none after the server gave up.
  PendingPuts pending;
  pending.add("a", 1000);
  pending.add("b", 2000);
  CHECK(pending.contains("a"));
  CHECK(pending.take("a"));
  CHECK(!pending.take("a"));
  pending.expire(2000 + PUT_REPLY_TIMEOUT_MS - 1);
  CHECK(pending.contains("b"));
  pending.expire(2000 + PUT_REPLY_TIMEOUT_MS);
  CHECK(!pending.contains("b"));
  // Wraparound of the millisecond clock.
  pending.add("c", 0xFFFFFF00u);
  pending.expire(0x100u);
  CHECK(pending.contains("c"));
  pending.clear();
  CHECK(pending.size() == 0);

  if (failures) {
    std::printf("%d failure(s)\n", failures);
    return 1;
  }
  std::printf("put_handling: all tests passed\n");
  return 0;
}
