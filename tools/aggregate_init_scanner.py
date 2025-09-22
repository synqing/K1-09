#!/usr/bin/env python3
"""
Aggregate-init safety scanner with optional .scannerignore.

Modes:
  strict : FAIL on flat-brace triples for CRGB16-like structs (C++17-safe).
  legacy : Allowed only with --allow-legacy; FAIL on explicit constructors inside aggregates.

Ignore controls (guardrail-only feature):
  - --ignore-file (default: .scannerignore)
  - Each non-empty, non-comment line is a glob for file paths to IGNORE ENTIRELY.
  - Inline per-site allow: place 'SCANNER:ALLOW_FLAT' on the same line as the initializer.

Exit codes:
  0 OK, 1 violations, 2 usage/config error
"""
import argparse
import fnmatch
import pathlib
import re
import sys

STRUCT_NAMES = [r"CRGB16", r"CRGBA16", r"Color16", r"Rgb16"]
FLOAT = r"[+-]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?"
FLAT_TRIPLE = re.compile(r"\{\s*(" + FLOAT + r")\s*,\s*(" + FLOAT + r")\s*,\s*(" + FLOAT + r")\s*\}")
FLAT_QUAD = re.compile(r"\{\s*(" + FLOAT + r")\s*,\s*(" + FLOAT + r")\s*,\s*(" + FLOAT + r")\s*,\s*(" + FLOAT + r")\s*\}")
EXPL_CONSTR = re.compile(r"SQ15x16\s*\(" + FLOAT + r"\)")
DECL_INIT = re.compile(
    rf"(?P<type>{'|'.join(STRUCT_NAMES)})\s+\w+\s*=\s*(?P<init>\{{.*?\}})\s*;",
    re.DOTALL,
)


def load_ignore_globs(ignore_file: str):
    path = pathlib.Path(ignore_file)
    if not path.exists():
        return []
    globs = []
    for raw in path.read_text(encoding="utf-8", errors="ignore").splitlines():
        entry = raw.strip()
        if not entry or entry.startswith("#"):
            continue
        globs.append(entry)
    return globs


def path_ignored(path: pathlib.Path, ignore_globs):
    rel = str(path)
    for pattern in ignore_globs:
        if fnmatch.fnmatch(rel, pattern):
            return True
    return False


def scan_file(path: pathlib.Path, mode: str):
    text = path.read_text(encoding="utf-8", errors="ignore")
    violations = []
    for match in DECL_INIT.finditer(text):
        init = match.group("init")
        line_start = text.rfind("\n", 0, match.start()) + 1
        line_end = text.find("\n", match.start(), match.end())
        if line_end == -1:
            line_end = match.end()
        line = text[line_start:line_end]
        if "SCANNER:ALLOW_FLAT" in line:
            continue

        if mode == "strict":
            flat_rgb = FLAT_TRIPLE.search(init) and "{{" not in init
            flat_rgba = FLAT_QUAD.search(init) and "{{" not in init
            if flat_rgb or flat_rgba:
                flavor = "RGB" if flat_rgb else "RGBA"
                violations.append(
                    (path, f"Flat-brace {flavor} under C++17; require nested braces or factory."),
                )
        else:
            if EXPL_CONSTR.search(init):
                violations.append(
                    (path, "Explicit SQ15x16(...) inside aggregate; preserve implicit list."),
                )
    return violations


def iter_sources(roots):
    exts = {".h", ".hpp", ".hh", ".ipp", ".cpp", ".cc", ".cxx", ".ino", ".c"}
    for root in roots:
        root_path = pathlib.Path(root)
        if not root_path.exists():
            print(f"[scanner] Missing root: {root}", file=sys.stderr)
            raise SystemExit(2)
        for path in root_path.rglob("*"):
            if path.suffix in exts:
                yield path


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--mode", choices=["strict", "legacy"], default="strict")
    parser.add_argument(
        "--allow-legacy",
        action="store_true",
        help="Required when --mode=legacy to avoid accidental use.",
    )
    parser.add_argument(
        "--ignore-file",
        default=".scannerignore",
        help="Path to ignore file with glob patterns (one per line).",
    )
    parser.add_argument("--roots", nargs="+", required=True)
    args = parser.parse_args()

    if args.mode == "legacy" and not args.allow_legacy:
        print("Refusing legacy scan without --allow-legacy.", file=sys.stderr)
        sys.exit(2)

    ignore_globs = load_ignore_globs(args.ignore_file)

    all_violations = []
    for path in iter_sources(args.roots):
        if path_ignored(path, ignore_globs):
            continue
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
