#!/usr/bin/env python3
"""
Header Cop — lightweight include hygiene checker for Phase 3.

Checks:
- No source files include a .cpp file via #include.
- No headers under include/ pull in project .cpp files.

Usage: python3 tools/header_cop.py [root]
Exit non‑zero on violations.
"""
import sys
from pathlib import Path
import re

ROOT = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parents[1]

inc_re = re.compile(r'^\s*#\s*include\s+[<\"]([^>\"]+)[>\"]')

violations = []

def scan_file(p: Path):
    try:
        txt = p.read_text(errors='ignore')
    except Exception:
        return
    for ln, line in enumerate(txt.splitlines(), 1):
        m = inc_re.match(line)
        if not m:
            continue
        target = m.group(1)
        if target.endswith('.cpp'):
            violations.append(f"{p}:{ln}: includes source file '{target}'")

def main():
    for p in ROOT.rglob('*.h'):
        scan_file(p)
    for p in ROOT.rglob('*.hpp'):
        scan_file(p)
    for p in ROOT.rglob('*.ino'):
        scan_file(p)
    # Also scan .cpp for pathological includes
    for p in ROOT.rglob('*.cpp'):
        scan_file(p)
    if violations:
        print('HEADER_COP VIOLATIONS:')
        for v in violations:
            print(' -', v)
        sys.exit(1)
    print('HEADER_COP: OK')

if __name__ == '__main__':
    main()

