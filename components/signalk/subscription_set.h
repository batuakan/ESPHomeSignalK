#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

namespace esphome {
namespace signalk {

/// One row of a SignalK subscribe message.
struct SubscriptionRow {
  std::string path;
  int period{1000};
  std::string format{"delta"};
  std::string policy{"instant"};
  int min_period{200};
};

/**
 * Ref-counted set of subscribed paths plus the set the server currently knows about.
 *
 * signalk-server only supports unsubscribing from everything
 * ({"context":"*","unsubscribe":[{"path":"*"}]}); a per-path unsubscribe makes it close the
 * WebSocket. So additions are sent incrementally, and any removal is applied as
 * "unsubscribe all, then subscribe the full set". Pure C++ so it can be unit-tested on the host.
 */
class SubscriptionSet {
 public:
  enum class Action { NONE, SUBSCRIBE_ADDED, RESUBSCRIBE_ALL };

  /// Add one reference to a path. The first reference defines the row's options.
  void add(const SubscriptionRow &row) {
    auto it = this->rows_.find(row.path);
    if (it == this->rows_.end()) {
      this->rows_.emplace(row.path, Entry{row, 1});
    } else {
      it->second.count++;
    }
  }

  /// Drop one reference; the path disappears when no references are left.
  void remove(const std::string &path) {
    auto it = this->rows_.find(path);
    if (it == this->rows_.end())
      return;
    if (--it->second.count == 0)
      this->rows_.erase(it);
  }

  bool contains(const std::string &path) const { return this->rows_.count(path) > 0; }
  size_t size() const { return this->rows_.size(); }

  /// What must be sent to bring the server in line with the current set.
  Action pending() const {
    for (const auto &path : this->synced_) {
      if (this->rows_.count(path) == 0)
        return Action::RESUBSCRIBE_ALL;
    }
    for (const auto &kv : this->rows_) {
      if (this->synced_.count(kv.first) == 0)
        return Action::SUBSCRIBE_ADDED;
    }
    return Action::NONE;
  }

  /// Rows the server does not have yet.
  std::vector<SubscriptionRow> added() const {
    std::vector<SubscriptionRow> out;
    for (const auto &kv : this->rows_) {
      if (this->synced_.count(kv.first) == 0)
        out.push_back(kv.second.row);
    }
    return out;
  }

  /// Every current row.
  std::vector<SubscriptionRow> all() const {
    std::vector<SubscriptionRow> out;
    out.reserve(this->rows_.size());
    for (const auto &kv : this->rows_)
      out.push_back(kv.second.row);
    return out;
  }

  /// The server now has exactly the current set.
  void mark_synced() {
    this->synced_.clear();
    for (const auto &kv : this->rows_)
      this->synced_.insert(kv.first);
  }

  /// A new connection starts with no subscriptions on the server side.
  void mark_disconnected() { this->synced_.clear(); }

 protected:
  struct Entry {
    SubscriptionRow row;
    int count;
  };
  std::map<std::string, Entry> rows_;
  std::set<std::string> synced_;
};

}  // namespace signalk
}  // namespace esphome
