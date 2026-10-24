#!/usr/bin/env bash
# Host C++ unit tests for logic that does not depend on ESPHome.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT
for src in "$ROOT_DIR"/tests/cpp/*_test.cpp; do
  name="$(basename "$src" .cpp)"
  g++ -std=c++17 -Wall -Wextra -Werror -O1 "$src" -o "$OUT/$name"
  echo "== $name"
  "$OUT/$name"
done
