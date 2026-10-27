#!/usr/bin/env bash
# Host C++ unit tests for logic that does not depend on ESPHome.
# Tests that need ArduinoJson look for it in $ARDUINOJSON_INCLUDE (a directory containing
# ArduinoJson.h), otherwise they are skipped.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT
for src in "$ROOT_DIR"/tests/cpp/*_test.cpp; do
  name="$(basename "$src" .cpp)"
  includes=()
  # AddressSanitizer/UBSan catch out of bounds reads in code that parses untrusted data. Not used for the
  # tests that include ArduinoJson: its headers trigger -Wmaybe-uninitialized once instrumented.
  sanitize=(-fsanitize=address,undefined)
  if grep -q "ArduinoJson.h" "$src"; then
    if [[ -z "${ARDUINOJSON_INCLUDE:-}" || ! -f "$ARDUINOJSON_INCLUDE/ArduinoJson.h" ]]; then
      echo "== $name: skipped (set ARDUINOJSON_INCLUDE to a directory containing ArduinoJson.h)"
      continue
    fi
    includes=(-I "$ARDUINOJSON_INCLUDE" -I "$ARDUINOJSON_INCLUDE/src")
    sanitize=()
  fi
  g++ -std=c++17 -Wall -Wextra -Werror -O1 "${sanitize[@]}" "${includes[@]}" "$src" -o "$OUT/$name"
  echo "== $name"
  "$OUT/$name"
done
