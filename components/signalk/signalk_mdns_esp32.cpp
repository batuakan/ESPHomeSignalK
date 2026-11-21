#ifdef USE_ESP32

#include "mdns.h"

#include <cstdio>
#include <cstring>

#include "esphome/core/log.h"
#include "signalk_mdns_esp32.h"

namespace esphome {
namespace signalk {

static const char *const TAG = "signalk";

// How long one search listens for answers, and the most servers kept from it.
static const uint32_t SEARCH_TIMEOUT_MS = 2500;
static const size_t SEARCH_MAX_RESULTS = 8;

SignalKMdnsEsp32::~SignalKMdnsEsp32() {
  if (search_ != nullptr)
    mdns_query_async_delete(static_cast<mdns_search_once_t *>(search_));
  if (search_tls_ != nullptr)
    mdns_query_async_delete(static_cast<mdns_search_once_t *>(search_tls_));
}

static ServerCandidate to_candidate(const mdns_result_t *r, bool tls) {
  ServerCandidate c;
  c.tls = tls;
  c.hostname = r->hostname != nullptr ? r->hostname : "";
  c.port = r->port;
  for (const mdns_ip_addr_t *a = r->addr; a != nullptr; a = a->next) {
    if (a->addr.type == ESP_IPADDR_TYPE_V4) {
      char buf[16];
      snprintf(buf, sizeof(buf), IPSTR, IP2STR(&a->addr.u_addr.ip4));
      c.address = buf;
      break;
    }
  }
  for (size_t i = 0; i < r->txt_count; i++) {
    const char *key = r->txt[i].key;
    const char *value = r->txt[i].value;
    if (key == nullptr || value == nullptr)
      continue;
    if (strcmp(key, "swname") == 0)
      c.swname = value;
    else if (strcmp(key, "roles") == 0)
      c.roles = value;
    else if (strcmp(key, "self") == 0)
      c.self = value;
  }
  return c;
}

/// Collects `search` into `found` once it has finished (and deletes it); false while it still runs.
static bool collect(void *&search, bool tls, std::vector<ServerCandidate> &found) {
  if (search == nullptr)
    return true;
  auto *s = static_cast<mdns_search_once_t *>(search);
  mdns_result_t *results = nullptr;
  uint8_t count = 0;
  if (!mdns_query_async_get_results(s, 0, &results, &count))
    return false;
  for (const mdns_result_t *r = results; r != nullptr; r = r->next)
    add_unique(found, to_candidate(r, tls));
  mdns_query_results_free(results);
  mdns_query_async_delete(s);
  search = nullptr;
  return true;
}

DiscoveryPoll SignalKMdnsEsp32::poll(std::vector<ServerCandidate> &found, bool tls) {
  found.clear();
  if (!started_) {
    // Fails until the network is up and mDNS has been started (by ESPHome's mdns component).
    search_ = mdns_query_async_new(nullptr, "_signalk-http", "_tcp", MDNS_TYPE_PTR, SEARCH_TIMEOUT_MS,
                                   SEARCH_MAX_RESULTS, nullptr);
    if (search_ == nullptr) {
      ESP_LOGD(TAG, "mDNS search could not be started yet (network or mDNS not ready)");
      return DiscoveryPoll::DONE;
    }
    if (tls) {
      search_tls_ = mdns_query_async_new(nullptr, "_signalk-https", "_tcp", MDNS_TYPE_PTR, SEARCH_TIMEOUT_MS,
                                         SEARCH_MAX_RESULTS, nullptr);
      if (search_tls_ == nullptr)
        ESP_LOGD(TAG, "mDNS search for servers with SSL could not be started");
    }
    started_ = true;
    found_.clear();
    return DiscoveryPoll::PENDING;
  }

  const bool plain_done = collect(search_, false, found_);
  const bool tls_done = collect(search_tls_, true, found_);
  if (!plain_done || !tls_done)
    return DiscoveryPoll::PENDING;
  started_ = false;
  found = std::move(found_);
  found_.clear();
  return DiscoveryPoll::DONE;
}

}  // namespace signalk
}  // namespace esphome

#endif
