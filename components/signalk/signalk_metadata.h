#pragma once

// Use ESPHome's header: it sets the ArduinoJson configuration macros before including the library, and
// including the library directly first would configure it differently from the rest of ESPHome.
// The fallback lets the host unit tests build without ESPHome.
#if __has_include("esphome/components/json/json_util.h")
#include "esphome/components/json/json_util.h"
#else
#include <ArduinoJson.h>
#endif
#include <string>
#include <vector>

/// Copies into `out` the entries of `ours` that `server` does not have.
/// Server values always win: a key the server has is never copied, even if the values differ.
/// Objects are compared key by key, everything else (scalars, arrays such as `zones`) is atomic.
inline void signalk_metadata_missing(JsonObjectConst ours, JsonObjectConst server, JsonObject out) {
  for (JsonPairConst kv : ours) {
    JsonVariantConst theirs = server[kv.key()];
    if (theirs.isNull()) {
      out[kv.key()] = kv.value();
    } else if (kv.value().is<JsonObjectConst>() && theirs.is<JsonObjectConst>()) {
      JsonObject nested = out[kv.key()].to<JsonObject>();
      signalk_metadata_missing(kv.value().as<JsonObjectConst>(), theirs.as<JsonObjectConst>(), nested);
      if (nested.size() == 0)
        out.remove(kv.key());
    }
  }
}

class SignalKMetadata {
 public:
  SignalKMetadata() {
    doc_.to<JsonObject>();  // Ensure root is object
  }

  /// Returns a writable JsonObject view
  JsonObject obj() { return doc_.as<JsonObject>(); }

  /// Returns a const JsonVariant for output
  JsonVariantConst view() const { return doc_.as<JsonVariantConst>(); }

  bool empty() const { return doc_.as<JsonObjectConst>().size() == 0; }

  /// Add/overwrite a simple value
  template<typename T> void set(const std::string &key, const T &value) { doc_[key.c_str()] = value; }

  /// Add array value
  void set_array(const std::string &key, const std::vector<std::string> &values) {
    JsonArray arr = doc_[key.c_str()].to<JsonArray>();
    for (auto &v : values)
      arr.add(v);
  }

  void set_json(const std::string &key, const std::string &json) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, json);
    if (error) {
      // Handle error (e.g., log it)
      return;
    }
    JsonVariant value = doc.as<JsonVariant>();
    doc_[key.c_str()] = value;
  }

  /// Get nested object, creating if not exist
  JsonObject ensure_object(const std::string &key) { return doc_[key.c_str()].to<JsonObject>(); }

  /// Copy metadata into outgoing JSON root/variant
  void to_json(JsonVariant dst) const { dst.set(doc_.as<JsonVariantConst>()); }

  /// Writes into `out` what the server lacks (see signalk_metadata_missing); `out` is left empty
  /// when the server already has every key.
  void missing_from(JsonVariantConst server, JsonObject out) const {
    signalk_metadata_missing(doc_.as<JsonObjectConst>(), server.as<JsonObjectConst>(), out);
  }

 private:
  JsonDocument doc_;
};
