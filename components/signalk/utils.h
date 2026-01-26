#pragma once

#include <time.h>
#include <cstdint>
#include <ctime>
#include <sys/time.h>
#include <string>

inline bool match_path(const std::string& path, const std::string& pattern) {
    if (pattern.back() == '*') {
        return path.rfind(pattern.substr(0, pattern.size() - 1), 0) == 0;
    }
    return path == pattern;
}

inline std::string generate_random_guid() {
  char guid[37];
  uint32_t rnd[4];
  for (int i = 0; i < 4; ++i) {
    rnd[i] = random();
  }
  snprintf(guid, sizeof(guid), "%08x-%04x-%04x-%04x-%04x%04x", rnd[0], rnd[1] >> 16, rnd[1] & 0xFFFF, rnd[2] >> 16,
           rnd[2] & 0xFFFF, rnd[3]);
  return std::string(guid);
}

inline std::string generate_requestid() {
  char guid[21];
  uint32_t rnd[4];
  for (int i = 0; i < 4; ++i) {
    rnd[i] = random();
  }
  snprintf(guid, sizeof(guid), "%06x-%06x-%06x", rnd[0] >> 16, rnd[1] >> 16, rnd[2] >> 16);
  return std::string(guid);
}

inline std::string iso_timestamp() {
  struct timeval tv;
  gettimeofday(&tv, nullptr);  // seconds + microseconds

  // Convert to UTC time
  struct tm *timeinfo = localtime(&tv.tv_sec);

  char buf[40];
  // Format: 2025-09-03T14:23:45.123Z
  snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d.%03ldZ", timeinfo->tm_year + 1900, timeinfo->tm_mon + 1,
           timeinfo->tm_mday, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec,
           tv.tv_usec / 1000);  // microseconds → milliseconds

  // ESP_LOGD(TAG, "Time: %s", buf);
  return std::string(buf);
}

#ifdef USE_HOST
#include <chrono>
#include <thread>
inline uint32_t sk_millis() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

inline std::string get_uuid() { return "12345678-1234-1232-123456789012"; }

#endif

#ifdef USE_ESP32_FRAMEWORK_ARDUINO
#include <Arduino.h>
inline uint32_t sk_millis() { return millis(); }

inline std::string get_uuid() {
  uint64_t mac = ESP.getEfuseMac();  // 48-bit unique chip ID (MAC)

  // Split into parts for UUID-like formatting
  char uuid[37];
  snprintf(uuid, sizeof(uuid), "%08lx-%04lx-%04lx-%04x-%012llx",
           (unsigned long) (mac >> 16),             // high 32 bits
           (unsigned long) ((mac >> 0) & 0xFFFF),   // next 16
           (unsigned long) ((mac >> 32) & 0xFFFF),  // next 16 (different shuffle, adjust as needed)
           0x0ba2,                                  // fixed chunk like in your code
           (unsigned long long) mac                 // low 48 bits as node identifier
  );

  return std::string(uuid);
}
#endif

#ifdef USE_ESP_IDF
#include "esp_log.h"
#include <esp_timer.h>
#include <esp_system.h>
#include <esp_mac.h>

inline uint32_t sk_millis() { return esp_timer_get_time() / 1000; }

inline std::string get_uuid() {
  uint8_t mac_bytes[6];
  esp_efuse_mac_get_default(mac_bytes);
  uint64_t mac = 0;
  for (int i = 0; i < 6; ++i) {
    mac |= ((uint64_t) mac_bytes[i]) << (8 * (5 - i));
  }
  char uuid[37];
  snprintf(uuid, sizeof(uuid), "%08x-%04x-%04x-%04x-%012llx", (uint32_t) (mac >> 32), (uint16_t) (mac >> 16) & 0xFFFF,
           (uint16_t) (mac) &0xFFFF,
           0x0ba2,  // could be anything fixed
           (unsigned long long) mac);
  return std::string(uuid);

  //   return "12345678-1234-1232-123456789012";
}

inline time_t timegm(struct tm *tm) {
  // Save and clear timezone offset
  char *tz = getenv("TZ");
  setenv("TZ", "", 1);  // force UTC
  tzset();

  time_t ret = mktime(tm);

  // Restore timezone
  if (tz)
    setenv("TZ", tz, 1);
  else
    unsetenv("TZ");
  tzset();

  return ret;
}

inline void set_time_from_iso8601(const std::string &iso_time) {
  struct tm tm = {};
  int milliseconds = 0;

  // Parse "YYYY-MM-DDTHH:MM:SS.mmmZ"
  if (sscanf(iso_time.c_str(), "%4d-%2d-%2dT%2d:%2d:%2d.%3dZ", &tm.tm_year, &tm.tm_mon, &tm.tm_mday, &tm.tm_hour,
             &tm.tm_min, &tm.tm_sec, &milliseconds) != 7) {
    // ESP_LOGE(TAG, "Failed to parse time string: %s", iso_time.c_str());
    return;
  }

  tm.tm_year -= 1900;  // struct tm expects years since 1900
  tm.tm_mon -= 1;      // struct tm expects 0–11

  // Convert to time_t (seconds since epoch, UTC)
  time_t epoch_sec = timegm(&tm);

  // Fill timeval with seconds + microseconds
  struct timeval now = {.tv_sec = epoch_sec, .tv_usec = milliseconds * 1000};

  // Set system time
  settimeofday(&now, nullptr);

  // ESP_LOGI(TAG, "System time set to: %s", iso_time.c_str());
}
#endif
