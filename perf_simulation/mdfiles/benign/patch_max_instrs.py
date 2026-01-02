#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
Recursively patch Python files under the current directory:
- Find a line that assigns max_total_instrs = 100000000 (whitespace-insensitive)
- Comment that line out
- Insert a new assignment line with value 1000000 immediately below
Default: dry-run. Use --apply to modify files.
"""

from __future__ import annotations

import argparse
import os
import re
import shutil
from pathlib import Path
from typing import List, Tuple


TARGET_VALUE = "100000000"
NEW_VALUE = "1000000"

# Matches lines like:
#   max_total_instrs               = 100000000
#   max_total_instrs=100000000
#   max_total_instrs    =    100000000   # comment
ASSIGN_RE = re.compile(
    r"^(\s*)(max_total_instrs)(\s*=\s*)(" + re.escape(TARGET_VALUE) + r")(\s*(#.*)?)\s*$"
)

# Detect if the immediately next non-empty line already sets it to NEW_VALUE
NEXTLINE_NEW_RE = re.compile(
    r"^\s*max_total_instrs\s*=\s*" + re.escape(NEW_VALUE) + r"(\s*(#.*)?)\s*$"
)


def patch_text(lines: List[str]) -> Tuple[bool, List[str]]:
    """
    Returns (changed, new_lines)
    """
    changed = False
    out: List[str] = []
    i = 0
    while i < len(lines):
        line = lines[i]
        m = ASSIGN_RE.match(line)
        if not m:
            out.append(line)
            i += 1
            continue

        indent, var, eq, _, trailing, _comment = m.groups()

        if re.match(r"^\s*#", line):
            out.append(line)
            i += 1
            continue
        
        commented = f"{indent}# {var}{eq}{TARGET_VALUE}{trailing}\n"
        out.append(commented)

        if i + 1 < len(lines) and NEXTLINE_NEW_RE.match(lines[i + 1]):
            # keep next line as-is
            i += 1
            changed = True
            continue

        inserted = f"{indent}{var}{eq}{NEW_VALUE}\n"
        out.append(inserted)

        changed = True
        i += 1

    return changed, out


def iter_py_files(root: Path) -> List[Path]:
    files: List[Path] = []
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = [
            d for d in dirnames
            if d not in {".git", "__pycache__", ".venv", "venv", "env", ".mypy_cache", ".pytest_cache"}
        ]
        for fn in filenames:
            if fn.endswith(".py"):
                files.append(Path(dirpath) / fn)
    return files


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--apply", action="store_true", default=True, help="Actually modify files (default: dry-run).")
    ap.add_argument("--backup", action="store_true", default=False, help="Create .bak backup files.")
    ap.add_argument("--root", default=".", help="Root directory to scan (default: current directory).")
    args = ap.parse_args()

    root = Path(args.root).resolve()
    py_files = iter_py_files(root)

    touched = 0
    changed_files: List[Path] = []

    for path in py_files:
        try:
            text = path.read_text(encoding="utf-8")
            lines = text.splitlines(keepends=True)
        except UnicodeDecodeError:
            text = path.read_text(encoding="latin-1")
            lines = text.splitlines(keepends=True)

        changed, new_lines = patch_text(lines)
        if not changed:
            continue

        touched += 1
        changed_files.append(path)

        if args.apply:
            if args.backup:
                bak = path.with_suffix(path.suffix + ".bak")
                shutil.copy2(path, bak)
            path.write_text("".join(new_lines), encoding="utf-8")

    if args.apply:
        print(f"[APPLIED] Modified {touched} file(s).")
    else:
        print(f"[DRY-RUN] Would modify {touched} file(s). Use --apply to write changes.")

    for p in changed_files:
        print(f" - {p}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
