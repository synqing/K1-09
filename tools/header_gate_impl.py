#!/usr/bin/env python3
"""
Mechanical gating of selected free-function definitions in a header.

Usage:
  python3 tools/header_gate_impl.py <header.h> <MACRO> <func1> <func2> ...

For each named function, the tool finds its top-level definition in the header and
replaces the body with:

#ifndef <MACRO>
<signature>;
#else
<original definition>
#endif

This keeps symbols identical while allowing a single translation unit to include the
header with <MACRO> defined so the bodies are emitted exactly once.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

def _decomment(source: str) -> str:
    out = []
    i = 0
    n = len(source)
    in_sl = False
    in_ml = False
    in_str: str | None = None
    while i < n:
        c = source[i]
        if in_str:
            out.append(c)
            if c == '\\' and i + 1 < n:
                out.append(source[i + 1])
                i += 2
                continue
            if c == in_str:
                in_str = None
            i += 1
            continue
        if in_sl:
            if c == '\n':
                in_sl = False
                out.append('\n')
            else:
                out.append(' ')
            i += 1
            continue
        if in_ml:
            if c == '*' and i + 1 < n and source[i + 1] == '/':
                out.append('  ')
                i += 2
                in_ml = False
            else:
                out.append('\n' if c == '\n' else ' ')
                i += 1
            continue
        if c == '"':
            in_str = '"'
            out.append(c)
            i += 1
            continue
        if c == "'":
            in_str = "'"
            out.append(c)
            i += 1
            continue
        if c == '/' and i + 1 < n and source[i + 1] == '/':
            in_sl = True
            out.append('  ')
            i += 2
            continue
        if c == '/' and i + 1 < n and source[i + 1] == '*':
            in_ml = True
            out.append('  ')
            i += 2
            continue
        out.append(c)
        i += 1
    return ''.join(out)

def _find_definition_span(src: str, func_name: str) -> tuple[int, int, int] | None:
    stripped = _decomment(src)
    pattern = re.compile(
        r'(^|\n)'
        r'(?P<sig>[^;\n{}]*?\b' + re.escape(func_name) + r'\s*\([^;{}]*\)\s*(?:noexcept\s*)?(?:const\s*)?(?:&?\s*)?)'
        r'\s*\{',
        re.MULTILINE,
    )
    match = next(iter(pattern.finditer(stripped)), None)
    if not match:
        return None
    sig_start = match.start('sig')
    body_start = stripped.find('{', match.end('sig') - 1)
    depth = 0
    i = body_start
    n = len(stripped)
    while i < n:
        c = stripped[i]
        if c == '{':
            depth += 1
        elif c == '}':
            depth -= 1
            if depth == 0:
                end = i + 1
                return (sig_start, body_start, end)
        i += 1
    return None

def _gate_function(src: str, name: str, macro: str) -> tuple[int, int, str] | None:
    span = _find_definition_span(src, name)
    if not span:
        return None
    sig_start, body_start, end = span
    signature = src[sig_start:body_start].rstrip()
    definition = src[sig_start:end]
    prototype = signature.rstrip()
    if not prototype.endswith(')'):
        idx = prototype.rfind(')')
        if idx != -1:
            prototype = prototype[:idx + 1]
    prototype = prototype + ';\n'
    gated = (
        f"#ifndef {macro}\n"
        f"{prototype}"
        f"#else\n"
        f"{definition}\n"
        f"#endif\n"
    )
    return (sig_start, end, ''.join(gated))

def main() -> None:
    if len(sys.argv) < 4:
        print(__doc__)
        sys.exit(2)
    header_path = Path(sys.argv[1])
    macro = sys.argv[2]
    func_names = sys.argv[3:]
    if not header_path.exists():
        print(f"ERR: header not found: {header_path}", file=sys.stderr)
        sys.exit(2)

    original = header_path.read_text(encoding='utf-8', errors='ignore')
    edits: list[tuple[int, int, str]] = []
    missing: list[str] = []
    for name in func_names:
        entry = _gate_function(original, name, macro)
        if entry is None:
            missing.append(name)
        else:
            edits.append(entry)

    if missing:
        print(
            "WARNING: missing function definitions not found in header: " + ", ".join(missing),
            file=sys.stderr,
        )

    edits.sort(key=lambda item: item[0], reverse=True)
    result = original
    for start, end, replacement in edits:
        result = result[:start] + replacement + result[end:]

    if result != original:
        backup = header_path.with_suffix(header_path.suffix + '.bak')
        backup.write_text(original, encoding='utf-8')
        header_path.write_text(result, encoding='utf-8')
        print(f"[gate] Wrote {header_path} (backup at {backup.name}); gated {len(edits)} function(s).")
    else:
        print("[gate] No changes made (nothing matched).")

    if missing and len(missing) == len(func_names):
        sys.exit(3)
    sys.exit(0)

if __name__ == '__main__':
    main()
