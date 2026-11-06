#pragma once

// Same ArduinoJson include as signalk_metadata.h: ESPHome's header first, the library for the host tests.
#if __has_include("esphome/components/json/json_util.h")
#include "esphome/components/json/json_util.h"
#else
#include <ArduinoJson.h>
#endif
#include <cstring>
#include <string>

namespace esphome {
namespace signalk {

/// The longest request link kept (also the size of its preference record). SignalK's are
/// "/signalk/v1/requests/<uuid>", 57 characters.
static const size_t ACCESS_REQUEST_HREF_MAX = 95;

/// What a SignalK reply to an access request (the POST, or a GET of its link) means for the device.
enum class AccessReply {
  PENDING,     // waiting for the admin; `href` is where to ask again
  APPROVED,    // `token` is the device's token
  DENIED,      // the admin denied it
  REFUSED,     // completed without a decision, e.g. "a device with clientId ... has already requested access"
               // while an earlier request is pending: its own link never gets a token
  GONE,        // the server doesn't know the request (it forgets them after an hour, or on a restart)
  UNREADABLE,  // not a reply we understand (a server error, not JSON)
};

struct AccessReplyInfo {
  AccessReply reply{AccessReply::UNREADABLE};
  std::string href;
  std::string token;
  std::string message;  // the server's reason for a refusal
};

/// A request link the device may poll: a path on the same server, short enough to keep.
inline bool usable_access_href(const std::string &href) {
  return !href.empty() && href.size() <= ACCESS_REQUEST_HREF_MAX && href.rfind("/signalk/", 0) == 0 &&
         href.find_first_of(" \r\n") == std::string::npos;
}

/// Reads a reply (HTTP status and body) to an access request or to a GET of its link.
inline AccessReplyInfo parse_access_reply(int http_status, const std::string &body) {
  AccessReplyInfo info;
  JsonDocument doc;
  if (http_status == 404 || deserializeJson(doc, body)) {
    // signalk-server answers an unknown request link with HTTP 500 "Unable to check request: not found".
    if (http_status == 404 || (http_status >= 400 && body.find("not found") != std::string::npos))
      info.reply = AccessReply::GONE;
    return info;
  }
  const char *state = doc["state"] | "";
  info.message = doc["message"] | "";
  if (std::strcmp(state, "PENDING") == 0) {
    info.href = doc["href"] | "";
    if (usable_access_href(info.href))
      info.reply = AccessReply::PENDING;
    return info;
  }
  if (std::strcmp(state, "COMPLETED") != 0)
    return info;
  const char *permission = doc["accessRequest"]["permission"] | "";
  if (std::strcmp(permission, "APPROVED") == 0) {
    info.token = doc["accessRequest"]["token"] | "";
    info.reply = info.token.empty() ? AccessReply::REFUSED : AccessReply::APPROVED;
  } else if (std::strcmp(permission, "DENIED") == 0) {
    info.reply = AccessReply::DENIED;
  } else {
    info.reply = AccessReply::REFUSED;
  }
  return info;
}

}  // namespace signalk
}  // namespace esphome
