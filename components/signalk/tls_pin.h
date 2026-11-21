#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <string>

namespace esphome {
namespace signalk {

/// SHA-256 of a server certificate (its DER encoding), as browsers and `openssl x509 -fingerprint -sha256`
/// show it.
using Fingerprint = std::array<uint8_t, 32>;

/// "8B:43:46:FD:..." for the first `bytes` bytes of `fp`.
inline std::string format_fingerprint(const Fingerprint &fp, size_t bytes = 32) {
  static const char *const DIGITS = "0123456789ABCDEF";
  std::string out;
  for (size_t i = 0; i < bytes && i < fp.size(); i++) {
    if (i > 0)
      out += ':';
    out += DIGITS[fp[i] >> 4];
    out += DIGITS[fp[i] & 0x0f];
  }
  return out;
}

/// How the device talks to its server, kept in an ESPHome preference next to the bound server: over TLS
/// or not, and the certificate it trusts there (pinned on first use). No record means plain HTTP and no
/// pin, as every device saved before TLS.
struct SavedTls {
  uint8_t version;  ///< SAVED_TLS_VERSION
  uint8_t tls;      ///< 0 or 1
  uint8_t pinned;   ///< 0 or 1; 1 only with tls
  uint8_t reserved;
  uint8_t sha256[32];
};
static const uint8_t SAVED_TLS_VERSION = 1;

inline SavedTls to_saved_tls(bool tls, bool pinned, const Fingerprint &pin) {
  SavedTls r{};
  r.version = SAVED_TLS_VERSION;
  r.tls = tls ? 1 : 0;
  r.pinned = tls && pinned ? 1 : 0;
  if (r.pinned)
    std::memcpy(r.sha256, pin.data(), pin.size());
  return r;
}

/// Reads a saved record. Anything else than a record written by to_saved_tls is refused and the outputs
/// are left as they were: flash can hold a damaged record.
inline bool from_saved_tls(const SavedTls &r, bool &tls, bool &pinned, Fingerprint &pin) {
  if (r.version != SAVED_TLS_VERSION || r.tls > 1 || r.pinned > 1 || r.reserved != 0 || (r.pinned && !r.tls))
    return false;
  tls = r.tls == 1;
  pinned = r.pinned == 1;
  pin.fill(0);
  if (pinned)
    std::memcpy(pin.data(), r.sha256, pin.size());
  return true;
}

/// The certificate the device trusts on its server. The server's own certificate is self-signed (and
/// signed with SHA-1, its name is "localhost"), so no CA and no name check can vouch for it: trust comes
/// from the exact certificate, learned on the first connection and required on every one after.
///
/// check() runs in whatever task makes a TLS connection (the WebSocket client task, the HTTP calls on the
/// main loop, other components' workers); the rest on the main loop.
class TlsTrust {
 public:
  /// The pin of the server now in use (nothing pinned: the next certificate is learned).
  void set(bool pinned, const Fingerprint &pin) {
    std::lock_guard<std::mutex> lock(mutex_);
    pinned_ = pinned;
    pin_ = pinned ? pin : Fingerprint{};
    learned_ = refused_ = false;
    ca_pem_.clear();
  }
  void clear() { set(false, Fingerprint{}); }

  /// True when a connection presenting a certificate with `got` may go on. Without a pin `got` becomes it.
  bool check(const Fingerprint &got) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!pinned_) {
      pinned_ = true;
      pin_ = got;
      learned_ = true;
      return true;
    }
    if (got == pin_)
      return true;
    refused_ = true;
    refused_fp_ = got;
    return false;
  }

  /// A pin learned since the last call (main loop: to save it).
  bool take_learned(Fingerprint &out) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!learned_)
      return false;
    learned_ = false;
    out = pin_;
    return true;
  }
  /// A certificate refused since the last call, and its fingerprint.
  bool take_refused(Fingerprint &out) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!refused_)
      return false;
    refused_ = false;
    out = refused_fp_;
    return true;
  }

  bool pinned() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return pinned_;
  }
  Fingerprint pin() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return pin_;
  }

  /// The pinned certificate as PEM, for clients that verify against a CA file (OpenSSL on the host build).
  /// Empty until a connection fetched it and check() accepted it; cleared with the pin.
  void set_ca_pem(std::string pem) {
    std::lock_guard<std::mutex> lock(mutex_);
    ca_pem_ = std::move(pem);
  }
  std::string ca_pem() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return ca_pem_;
  }

 private:
  mutable std::mutex mutex_;
  bool pinned_{false};
  Fingerprint pin_{};
  bool learned_{false};
  bool refused_{false};
  Fingerprint refused_fp_{};
  std::string ca_pem_;
};

/// The device's one trust store (there is one SignalK server per device). A function, so that the TLS
/// verify callbacks, which get no context of their own (crt_bundle_attach), can reach it.
inline TlsTrust &tls_trust() {
  static TlsTrust trust;
  return trust;
}

}  // namespace signalk
}  // namespace esphome
