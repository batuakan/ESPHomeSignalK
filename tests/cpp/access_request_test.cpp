// Host unit test for reading SignalK's access request replies (needs ArduinoJson): tools/test_cpp.sh
// The bodies are what signalk-server 2.36 sends.
#include <cstdio>
#include <string>

#include <ArduinoJson.h>

#include "../../components/signalk/access_request.h"

using namespace esphome::signalk;

static int failures = 0;
#define CHECK(cond) \
  do { \
    if (!(cond)) { \
      std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      failures++; \
    } \
  } while (0)

static const char *HREF = "/signalk/v1/requests/b32aaf7f-3429-465c-a622-00976fc7f439";

int main() {
  // The first request: pending, with the link to ask again.
  AccessReplyInfo r = parse_access_reply(
      202, "{\"state\":\"PENDING\",\"requestId\":\"b32aaf7f-3429-465c-a622-00976fc7f439\",\"statusCode\":202,"
           "\"href\":\"/signalk/v1/requests/b32aaf7f-3429-465c-a622-00976fc7f439\",\"ip\":\"::1\",\"user\":null}");
  CHECK(r.reply == AccessReply::PENDING);
  CHECK(r.href == HREF);

  // A second request while the first is pending: refused, with a link of its own that never gets a token.
  r = parse_access_reply(400, "{\"state\":\"COMPLETED\",\"requestId\":\"b3972d52\",\"statusCode\":400,"
                              "\"message\":\"A device with clientId 'x' has already requested access\","
                              "\"href\":\"/signalk/v1/requests/b3972d52\"}");
  CHECK(r.reply == AccessReply::REFUSED);
  CHECK(r.message == "A device with clientId 'x' has already requested access");
  // Polling that link gives the same refusal, now with HTTP 200.
  r = parse_access_reply(200, "{\"state\":\"COMPLETED\",\"statusCode\":400,\"message\":\"already requested\"}");
  CHECK(r.reply == AccessReply::REFUSED);

  // Device requests switched off on the server.
  r = parse_access_reply(403, "{\"state\":\"COMPLETED\",\"statusCode\":403}");
  CHECK(r.reply == AccessReply::REFUSED);

  // Still pending.
  r = parse_access_reply(200, std::string("{\"state\":\"PENDING\",\"href\":\"") + HREF + "\"}");
  CHECK(r.reply == AccessReply::PENDING);

  // Approved, with the token.
  r = parse_access_reply(200, "{\"state\":\"COMPLETED\",\"statusCode\":200,"
                              "\"accessRequest\":{\"permission\":\"APPROVED\",\"token\":\"t0k\"}}");
  CHECK(r.reply == AccessReply::APPROVED);
  CHECK(r.token == "t0k");
  // Approved without a token can't be used.
  r = parse_access_reply(200, "{\"state\":\"COMPLETED\",\"accessRequest\":{\"permission\":\"APPROVED\"}}");
  CHECK(r.reply == AccessReply::REFUSED);

  r = parse_access_reply(200, "{\"state\":\"COMPLETED\",\"accessRequest\":{\"permission\":\"DENIED\"}}");
  CHECK(r.reply == AccessReply::DENIED);

  // The server forgot the request (an hour later, or after a restart): what 2.36 sends, and a plain 404.
  CHECK(parse_access_reply(500, "Unable to check request: not found").reply == AccessReply::GONE);
  CHECK(parse_access_reply(404, "Not Found").reply == AccessReply::GONE);

  // Nothing we understand.
  CHECK(parse_access_reply(500, "Internal Server Error").reply == AccessReply::UNREADABLE);
  CHECK(parse_access_reply(200, "{\"state\":\"SOMETHING\"}").reply == AccessReply::UNREADABLE);
  CHECK(parse_access_reply(200, "[1,2]").reply == AccessReply::UNREADABLE);
  CHECK(parse_access_reply(200, "").reply == AccessReply::UNREADABLE);
  // A pending reply with a link we won't poll.
  CHECK(parse_access_reply(202, "{\"state\":\"PENDING\"}").reply == AccessReply::UNREADABLE);
  CHECK(parse_access_reply(202, "{\"state\":\"PENDING\",\"href\":\"http://elsewhere/x\"}").reply ==
        AccessReply::UNREADABLE);

  // Links kept in the preference record.
  CHECK(usable_access_href(HREF));
  CHECK(!usable_access_href(""));
  CHECK(!usable_access_href("signalk/v1/requests/x"));
  CHECK(!usable_access_href("/signalk/v1/requests/x y"));
  CHECK(!usable_access_href("/signalk/" + std::string(ACCESS_REQUEST_HREF_MAX, 'a')));
  CHECK(usable_access_href("/signalk/" + std::string(ACCESS_REQUEST_HREF_MAX - 9, 'a')));

  if (failures == 0)
    std::printf("access_request_test: all passed\n");
  return failures == 0 ? 0 : 1;
}
