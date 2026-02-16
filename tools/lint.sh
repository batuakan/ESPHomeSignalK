#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VENV_DIR="$ROOT_DIR/.venv"
VENV_PY="$VENV_DIR/bin/python"
VENV_RUFF="$VENV_DIR/bin/ruff"
VENV_YAMLLINT="$VENV_DIR/bin/yamllint"

if [ ! -x "$VENV_PY" ]; then
  python3 -m venv "$VENV_DIR"
fi

"$VENV_PY" -m pip install --upgrade pip
"$VENV_PY" -m pip install -r "$ROOT_DIR/requirements-dev.txt"

"$VENV_PY" "$ROOT_DIR/tools/lint_esphome_like.py"

"$VENV_RUFF" format --check "$ROOT_DIR/components" "$ROOT_DIR/tools"
"$VENV_RUFF" check "$ROOT_DIR/components" "$ROOT_DIR/tools"

"$VENV_YAMLLINT" -c "$ROOT_DIR/.yamllint" "$ROOT_DIR/examples" "$ROOT_DIR/components" "$ROOT_DIR/.github"

if ! command -v clang-format >/dev/null 2>&1; then
  echo "clang-format not found. Please install clang-format." >&2
  exit 1
fi

mapfile -t CPP_FILES < <(find "$ROOT_DIR/components" -type f \( -name "*.cpp" -o -name "*.h" \) | sort)
if [ ${#CPP_FILES[@]} -eq 0 ]; then
  exit 0
fi

clang-format -n --Werror "${CPP_FILES[@]}"
