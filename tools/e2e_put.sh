#!/usr/bin/env bash
# End-to-end test of PUT handling: builds tests/e2e/put_host.yaml for the host, runs it against a throwaway
# signalk-server (own config dir, port 3300, no mDNS, no NMEA/TCP listeners) and sends PUTs over REST.
# Needs: signalk-server on PATH, esphome (ESPHOME=/path/to/esphome), ixwebsocket for the host build, curl, jq.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ESPHOME="${ESPHOME:-esphome}"
PORT=3300
BASE="http://127.0.0.1:$PORT"
WORK="$(mktemp -d)"
PIDS=()
cleanup() {
  for p in "${PIDS[@]}"; do kill "$p" 2>/dev/null || true; done
  wait 2>/dev/null || true
  rm -rf "$WORK"
}
trap cleanup EXIT

mkdir -p "$WORK/sk"
cat >"$WORK/sk/settings.json" <<JSON
{"interfaces": {"nmea-tcp": false, "tcp": false, "mdns": false}, "mdns": false, "pipedProviders": [],
 "security": {"strategy": "./tokensecurity"}}
JSON
echo '{}' >"$WORK/sk/package.json"

echo "== building device"
"$ESPHOME" compile "$ROOT_DIR/tests/e2e/put_host.yaml" >"$WORK/build.log" 2>&1 || { tail -40 "$WORK/build.log"; exit 1; }
BIN="$ROOT_DIR/tests/e2e/.esphome/build/sk-put-e2e/.pioenvs/sk-put-e2e/program"

echo "== starting signalk-server on $PORT"
SIGNALK_NODE_CONFIG_DIR="$WORK/sk" PORT=$PORT ADMINUSER=admin:e2e-secret signalk-server >"$WORK/server.log" 2>&1 &
PIDS+=($!)
for _ in $(seq 60); do curl -sf "$BASE/signalk" >/dev/null && break; sleep 1; done

TOKEN=$(curl -sf -X POST -H 'Content-Type: application/json' -d '{"username":"admin","password":"e2e-secret"}' \
  "$BASE/signalk/v1/auth/login" | jq -r .token)

echo "== starting device"
ESPHOME_PREFDIR="$WORK/prefs" "$BIN" >"$WORK/device.log" 2>&1 &
PIDS+=($!)
for _ in $(seq 30); do
  curl -sf -H "Authorization: Bearer $TOKEN" "$BASE/signalk/v1/api/vessels/self/test/put/switch" >/dev/null && break
  sleep 1
done

fail=0
# put PATH VALUE EXPECTED_STATUS [EXPECTED_MESSAGE]: PUT over REST and follow the request until it completes.
put() {
  local path="$1" value="$2" want="$3" want_msg="${4:-}"
  local res href state code msg
  res=$(curl -s -X PUT -H "Authorization: Bearer $TOKEN" -H 'Content-Type: application/json' \
    -d "{\"value\": $value}" "$BASE/signalk/v1/api/vessels/self/${path//.//}")
  state=$(jq -r .state <<<"$res")
  href=$(jq -r '.href // empty' <<<"$res")
  for _ in $(seq 20); do
    [[ "$state" != "PENDING" || -z "$href" ]] && break
    sleep 0.25
    res=$(curl -s -H "Authorization: Bearer $TOKEN" "$BASE$href")
    state=$(jq -r .state <<<"$res")
  done
  code=$(jq -r .statusCode <<<"$res")
  msg=$(jq -r '.message // ""' <<<"$res")
  if [[ "$code" == "$want" && ( -z "$want_msg" || "$msg" == "$want_msg" ) ]]; then
    echo "ok   PUT $path=$value -> $code $msg"
  else
    echo "FAIL PUT $path=$value -> $code '$msg' (want $want '$want_msg')"
    fail=1
  fi
}
value() { curl -s -H "Authorization: Bearer $TOKEN" "$BASE/signalk/v1/api/vessels/self/${1//.//}/value"; }

put test.put.steps 5 200
sleep 0.5
[[ "$(value test.put.steps)" == "5" ]] && echo "ok   test.put.steps is 5" || { echo "FAIL test.put.steps is $(value test.put.steps)"; fail=1; }
put test.put.steps 500 400 "Too many"
put test.put.mode true 200 "on"
put test.put.mode false 200 "off"
put test.put.switch true 200
sleep 0.5
[[ "$(value test.put.switch)" == "true" ]] && echo "ok   switch is on" || { echo "FAIL switch is $(value test.put.switch)"; fail=1; }
put test.put.nobody 1 405

if [[ $fail -ne 0 ]]; then
  echo "--- device log"; tail -30 "$WORK/device.log"
  echo "--- server log"; tail -30 "$WORK/server.log"
  exit 1
fi
echo "all PUT end-to-end checks passed"
