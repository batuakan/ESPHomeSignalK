#pragma once

// Same ArduinoJson include as signalk_metadata.h: ESPHome's header first, the library for the host tests.
#if __has_include("esphome/components/json/json_util.h")
#include "esphome/components/json/json_util.h"
#else
#include <ArduinoJson.h>
#endif
#include <cstdint>
#include <string>
#include <vector>

namespace esphome {
namespace signalk {

// signalk-server forwards a PUT to the WebSocket client that publishes the path (a delta with `$source`)
// when no plugin handles it, as {"requestId": ..., "context": ..., "put": [{"path": ..., "value": ...}]}.
// The client answers {"requestId": ..., "state": "COMPLETED", "statusCode": ...}; the server gives up with
// 504 after 60 seconds.

/// How long a forwarded PUT can still be answered: the server's own timeout.
static const uint32_t PUT_REPLY_TIMEOUT_MS = 60000;

/// One `put` entry of a forwarded request. `value` points into the parsed document.
struct PutItem {
  std::string path;
  JsonVariantConst value;
};

struct ForwardedPut {
  std::string request_id;
  std::string context;
  std::vector<PutItem> items;
};

/// True when `doc` is a PUT forwarded by the server. `put` may be one object or an array of them; entries
/// without a path are skipped.
inline bool parse_forwarded_put(JsonVariantConst doc, ForwardedPut &out) {
  if (!doc["requestId"].is<const char *>())
    return false;
  JsonVariantConst put = doc["put"];
  if (!put.is<JsonArrayConst>() && !put.is<JsonObjectConst>())
    return false;
  out.request_id = doc["requestId"].as<std::string>();
  out.context = doc["context"].is<const char *>() ? doc["context"].as<std::string>() : std::string("vessels.self");
  out.items.clear();
  auto add = [&out](JsonVariantConst entry) {
    if (entry["path"].is<const char *>())
      out.items.push_back(PutItem{entry["path"].as<std::string>(), entry["value"]});
  };
  if (put.is<JsonArrayConst>()) {
    for (JsonVariantConst entry : put.as<JsonArrayConst>())
      add(entry);
  } else {
    add(put);
  }
  return true;
}

/// The reply to a forwarded PUT. `message` is left out when empty.
inline std::string put_reply_json(const std::string &request_id, int status_code, const std::string &message) {
  JsonDocument doc;
  doc["requestId"] = request_id;
  doc["state"] = status_code == 202 ? "PENDING" : "COMPLETED";
  doc["statusCode"] = status_code;
  if (!message.empty())
    doc["message"] = message;
  std::string out;
  serializeJson(doc, out);
  return out;
}

/// The PUT value as text for a string handler: a string as it is, anything else as JSON (`null` when missing).
inline std::string put_value_text(JsonVariantConst value) {
  if (value.is<const char *>())
    return value.as<std::string>();
  std::string out;
  serializeJson(value, out);
  return out;
}

/// PUT value as a number: numbers, and booleans as 0/1. False for anything else.
inline bool put_value_number(JsonVariantConst value, double &out) {
  if (value.is<bool>()) {
    out = value.as<bool>() ? 1.0 : 0.0;
    return true;
  }
  if (value.is<double>() || value.is<long>()) {
    out = value.as<double>();
    return true;
  }
  return false;
}

/// PUT value as a boolean: true/false, numbers (non zero is true) and the strings "true"/"on"/"false"/"off".
inline bool put_value_bool(JsonVariantConst value, bool &out) {
  if (value.is<bool>()) {
    out = value.as<bool>();
    return true;
  }
  if (value.is<double>() || value.is<long>()) {
    out = value.as<double>() != 0.0;
    return true;
  }
  if (value.is<const char *>()) {
    std::string s = value.as<std::string>();
    if (s == "true" || s == "on" || s == "ON" || s == "1") {
      out = true;
      return true;
    }
    if (s == "false" || s == "off" || s == "OFF" || s == "0") {
      out = false;
      return true;
    }
  }
  return false;
}

/// Forwarded PUTs that still need a reply, so that each gets exactly one and late replies are dropped.
class PendingPuts {
 public:
  void add(const std::string &request_id, uint32_t now) {
    expire(now);
    entries_.push_back(Entry{request_id, now});
  }
  /// True (and forgotten) when `request_id` is waiting for its reply.
  bool take(const std::string &request_id) {
    for (auto it = entries_.begin(); it != entries_.end(); ++it) {
      if (it->request_id == request_id) {
        entries_.erase(it);
        return true;
      }
    }
    return false;
  }
  bool contains(const std::string &request_id) const {
    for (const auto &e : entries_)
      if (e.request_id == request_id)
        return true;
    return false;
  }
  /// Drops requests the server has given up on.
  void expire(uint32_t now) {
    for (auto it = entries_.begin(); it != entries_.end();) {
      if (now - it->since >= PUT_REPLY_TIMEOUT_MS)
        it = entries_.erase(it);
      else
        ++it;
    }
  }
  /// Forgets everything: after a disconnect no reply can reach the server.
  void clear() { entries_.clear(); }
  size_t size() const { return entries_.size(); }

 private:
  struct Entry {
    std::string request_id;
    uint32_t since;
  };
  std::vector<Entry> entries_;
};

}  // namespace signalk
}  // namespace esphome
