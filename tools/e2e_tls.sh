#!/usr/bin/env bash
# End-to-end test of TLS: builds tests/e2e/tls_host.yaml for the host and runs it against a throwaway
# signalk-server with SSL turned on (own config dir, redirect port 3310, HTTPS/WSS on 3311, no mDNS, no
# NMEA/TCP listeners). Checks that the device pins the server's own certificate on first use, logs in and
# streams over WSS, keeps the pin after a restart, and refuses the server once its certificate changes.
# Needs: signalk-server on PATH, esphome (ESPHOME=/path/to/esphome), ixwebsocket for the host build, curl,
# jq, openssl.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ESPHOME="${ESPHOME:-esphome}"
PORT=3310
SSLPORT=3311
BASE="https://127.0.0.1:$SSLPORT"
WORK="$(mktemp -d)"
SERVER_PID=""
DEVICE_PID=""
cleanup() {
  for p in "$DEVICE_PID" "$SERVER_PID"; do [[ -n "$p" ]] && kill "$p" 2>/dev/null || true; done
  wait 2>/dev/null || true
  rm -rf "$WORK"
}
trap cleanup EXIT

mkdir -p "$WORK/sk"
cat >"$WORK/sk/settings.json" <<JSON
{"interfaces": {"nmea-tcp": false, "tcp": false, "mdns": false}, "mdns": false, "pipedProviders": [],
 "ssl": true, "port": $PORT, "sslport": $SSLPORT, "security": {"strategy": "./tokensecurity"}}
JSON
echo '{}' >"$WORK/sk/package.json"

echo "== building device"
"$ESPHOME" compile "$ROOT_DIR/tests/e2e/tls_host.yaml" >"$WORK/build.log" 2>&1 || { tail -40 "$WORK/build.log"; exit 1; }
BIN="$ROOT_DIR/tests/e2e/.esphome/build/sk-tls-e2e/.pioenvs/sk-tls-e2e/program"

start_server() {
  SIGNALK_NODE_CONFIG_DIR="$WORK/sk" PORT=$PORT SSLPORT=$SSLPORT ADMINUSER=admin:e2e-secret signalk-server \
    >>"$WORK/server.log" 2>&1 &
  SERVER_PID=$!
  for _ in $(seq 60); do curl -skf "$BASE/signalk" >/dev/null && return 0; sleep 1; done
  echo "FAIL signalk-server did not start"; tail -20 "$WORK/server.log"; exit 1
}
stop() { kill "$1" 2>/dev/null || true; wait "$1" 2>/dev/null || true; }
start_device() {
  : >"$WORK/device.log"
  ESPHOME_PREFDIR="$WORK/prefs" "$BIN" >"$WORK/device.log" 2>&1 &
  DEVICE_PID=$!
}
# wait_log PATTERN SECONDS: true once the device log has PATTERN.
wait_log() {
  for _ in $(seq "$2"); do grep -q -- "$1" "$WORK/device.log" && return 0; sleep 1; done
  return 1
}
fingerprint() { openssl x509 -in "$WORK/sk/ssl-cert.pem" -noout -fingerprint -sha256 | cut -d= -f2; }

fail=0
ok() { echo "ok   $1"; }
bad() { echo "FAIL $1"; fail=1; }

echo "== starting signalk-server with SSL on $SSLPORT"
start_server
FP=$(fingerprint)
TOKEN=$(curl -skf -X POST -H 'Content-Type: application/json' -d '{"username":"admin","password":"e2e-secret"}' \
  "$BASE/signalk/v1/auth/login" | jq -r .token)

echo "== first start: pins the certificate"
start_device
wait_log "Pinned the certificate of SignalK server 127.0.0.1:$SSLPORT, SHA-256 $FP" 30 &&
  ok "pinned the server's own certificate ($FP)" || bad "no pin of $FP"
wait_log "Connected to SignalK stream" 30 && ok "stream connected over WSS" || bad "stream not connected"
wait_log "e2e: test.tls.value is 42" 15 && ok "a delta went out and came back over WSS" || bad "no delta over WSS"
value=$(curl -sk -H "Authorization: Bearer $TOKEN" "$BASE/signalk/v1/api/vessels/self/test/tls/value/value")
[[ "$value" == "42" ]] && ok "the server has the published value" || bad "server value is '$value'"

echo "== restart: the pin is kept"
stop "$DEVICE_PID"
start_device
wait_log "Connected to SignalK stream" 30 && ok "connected again with the saved pin" || bad "no reconnect"
grep -q "Pinned the certificate" "$WORK/device.log" && bad "pinned again after a restart" || ok "not pinned again"

echo "== the server gets another certificate: refused"
stop "$DEVICE_PID"
stop "$SERVER_PID"
rm -f "$WORK/sk/ssl-cert.pem" "$WORK/sk/ssl-key.pem"
start_server
NEW=$(fingerprint)
[[ "$NEW" != "$FP" ]] || bad "the server kept its certificate"
start_device
wait_log "presented another certificate: SHA-256 $NEW, pinned $FP" 30 &&
  ok "refused the new certificate ($NEW)" || bad "the new certificate was not refused"
sleep 3
grep -q "Connected to SignalK stream" "$WORK/device.log" && bad "connected despite the changed certificate" ||
  ok "nothing connected to the server with the changed certificate"

if [[ $fail -ne 0 ]]; then
  echo "--- device log"; tail -40 "$WORK/device.log"
  echo "--- server log"; tail -20 "$WORK/server.log"
  exit 1
fi
echo "all TLS end-to-end checks passed"
