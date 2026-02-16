#!/usr/bin/env python3
from __future__ import annotations

from collections.abc import Iterable
import os
from pathlib import Path
import re
import sys

ROOT_DIR = Path(__file__).resolve().parents[1]
TARGET_DIR = ROOT_DIR / "components" / "signalk"

FORBIDDEN_ARDUINO_FUNCS = [
    "digitalWrite",
    "digitalRead",
    "pinMode",
    "shiftOut",
    "shiftIn",
    "analogRead",
    "analogWrite",
    "pulseIn",
    "pulseInLong",
    "tone",
]

CPP_EXTENSIONS = {".h", ".hpp", ".cpp", ".c", ".cc", ".cxx", ".tcc"}


def iter_files(base_dir: Path) -> Iterable[Path]:
    for root, _, files in os.walk(base_dir):
        for name in files:
            path = Path(root) / name
            yield path


def pos_to_line_col(text: str, pos: int) -> tuple[int, int]:
    line = text.count("\n", 0, pos) + 1
    last_nl = text.rfind("\n", 0, pos)
    col = pos + 1 if last_nl == -1 else pos - last_nl
    return line, col


def add_error(errors: list[str], path: Path, line: int, col: int, msg: str) -> None:
    rel = path.relative_to(ROOT_DIR)
    errors.append(f"{rel}:{line}:{col}: lint: {msg}")


def check_whitespace(errors: list[str], path: Path, content: str) -> None:
    if "\r" in content:
        pos = content.find("\r")
        line, col = pos_to_line_col(content, pos)
        add_error(errors, path, line, col, "Windows newline detected (CRLF)")

    lines = content.split("\n")
    for index, line_text in enumerate(lines, start=1):
        tab_pos = line_text.find("\t")
        if tab_pos != -1:
            add_error(errors, path, index, tab_pos + 1, "Tab character detected")
        match = re.search(r"[ \t]+$", line_text)
        if match:
            add_error(errors, path, index, match.start() + 1, "Trailing whitespace detected")


def check_pragma_once(errors: list[str], path: Path, content: str) -> None:
    if "#pragma once" not in content:
        add_error(errors, path, 1, 1, "Header missing #pragma once")


def check_cpp_rules(errors: list[str], path: Path, content: str) -> None:
    if re.search(r"#\s*include\s+[<\"]esphome\.h[>\"]", content):
        add_error(errors, path, 1, 1, "Do not include esphome.h; include specific headers instead")

    for func in FORBIDDEN_ARDUINO_FUNCS:
        for match in re.finditer(rf"\b{re.escape(func)}\s*\(", content):
            line, col = pos_to_line_col(content, match.start())
            add_error(
                errors,
                path,
                line,
                col,
                f"Forbidden Arduino function call: {func}()",
            )

    for match in re.finditer(r"\bdelay\s*\(\s*(\d+)\s*\)", content):
        duration_ms = int(match.group(1))
        if duration_ms >= 50:
            line, col = pos_to_line_col(content, match.start())
            add_error(errors, path, line, col, "delay() >= 50ms is not allowed")

    for match in re.finditer(r"\bstd\s*::\s*to_string\s*\(", content):
        line, col = pos_to_line_col(content, match.start())
        add_error(errors, path, line, col, "std::to_string() allocates heap memory")

    for match in re.finditer(r"\b(v?sprintf)\s*\(", content):
        line, col = pos_to_line_col(content, match.start())
        add_error(errors, path, line, col, "sprintf/vsprintf is not allowed; use snprintf")

    for match in re.finditer(r"\b(?:v?f?scanf|v?sscanf|scanf)\s*\(", content):
        line, col = pos_to_line_col(content, match.start())
        add_error(errors, path, line, col, "scanf family is not allowed")


def main() -> int:
    if not TARGET_DIR.exists():
        print("components/signalk not found", file=sys.stderr)
        return 2

    errors: list[str] = []

    for path in iter_files(TARGET_DIR):
        if path.suffix.lower() not in CPP_EXTENSIONS and path.suffix.lower() not in {".py", ".yaml", ".yml"}:
            continue

        try:
            content = path.read_text(encoding="utf-8")
        except UnicodeDecodeError:
            add_error(errors, path, 1, 1, "File is not UTF-8")
            continue

        check_whitespace(errors, path, content)

        if path.suffix.lower() in CPP_EXTENSIONS:
            check_cpp_rules(errors, path, content)
            if path.suffix.lower() in {".h", ".hpp"}:
                check_pragma_once(errors, path, content)

    if errors:
        for err in errors:
            print(err)
        return 1

    print("lint_esphome_like: OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
