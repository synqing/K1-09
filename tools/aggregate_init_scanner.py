#!/usr/bin/env python3
"""
Aggregate-init safety scanner for FixedPoints/CRGB16 patterns.

Policy modes:
  - strict  : FAIL on flat-brace float triples for CRGB16-like structs (C++17-safe).
  - legacy  : FAIL on explicit constructors inside aggregates (preserve implicit lists).

Rationale:
  * C++17 aggregate init can bypass FixedPoints constructors -> corrupted values.
    See debrief for evidence and binary diffs. Nested braces or factory paths are safe.
  * During header-splitting, do NOT "pretty up" initializers without understanding the semantics.

Usage:
  python tools/aggregate_init_scanner.py --mode=strict --roots src include lib

Exit code:
  0 = OK, 1 = violations found
"""
import argparse
import pathlib
import re
import sys

STRUCT_NAMES = [r"CRGB16", r"CRGBA16", r"Color16", r"Rgb16"]
FLOAT = r"[+-]?(?:\\d+(?:\\.\\d*)?|\\.\\d+)(?:[eE][+-]?\\d+)?"
FLAT_TRIPLE = re.compile(r"\\{\\s*(" + FLOAT + r")\\s*,\\s*(" + FLOAT + r")\\s*,\\s*(" + FLOAT + r")\\s*\\}")
EXPL_CONSTR = re.compile(r"SQ15x16\\s*\\(" + FLOAT + r"\\)")
DECL_INIT = re.compile(rf"(?P<type>{'|'.join(STRUCT_NAMES)})\\s+\\w+\\s*=\\s*(?P<init>\\{{.*?\\}})\\s*;", re.DOTALL)


def scan_file(path: pathlib.Path, mode: str):
    txt = path.read_text(encoding="utf-8", errors="ignore")
    violations = []

    for match in DECL_INIT.finditer(txt):
        init = match.group("init")
        if mode == "strict":
            if FLAT_TRIPLE.search(init) and "{{" not in init:
                violations.append((path, "Flat-brace triple detected under C++17; require nested braces or safe factory."))
        elif mode == "legacy":
            if EXPL_CONSTR.search(init):
                violations.append((path, "Explicit SQ15x16(...) constructor inside aggregate; preserve implicit list exactly."))
    return violations


def iter_sources(roots):
    exts = {".h", ".hpp", ".hh", ".ipp", ".cpp", ".cc", ".cxx", ".ino", ".c"}
    for root in roots:
        for path in pathlib.Path(root).rglob("*"):
            if path.suffix in exts:
                yield path


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--mode", choices=["strict", "legacy"], default="strict")
    parser.add_argument("--roots", nargs="+", required=True)
    args = parser.parse_args()

    all_violations = []
    for path in iter_sources(args.roots):
        all_violations.extend(scan_file(path, args.mode))

    if all_violations:
        print("AGGREGATE-INIT VIOLATIONS:")
        for path, message in all_violations:
            print(f" - {path}: {message}")
        sys.exit(1)
    print("Aggregate-init scan: OK")
    sys.exit(0)


if __name__ == "__main__":
    main()
