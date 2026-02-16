#pragma once

#include <ArduinoJson.h>
#include <string>

class SignalKMetadata {
 public:
  SignalKMetadata() {
    doc_.to<JsonObject>();  // Ensure root is object
  }

  /// Returns a writable JsonObject view
  JsonObject obj() { return doc_.as<JsonObject>(); }

  /// Returns a const JsonVariant for output
  JsonVariantConst view() const { return doc_.as<JsonVariantConst>(); }

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

 private:
  JsonDocument doc_;
};