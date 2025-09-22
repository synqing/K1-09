#!/usr/bin/env python3
"""
Mechanical header splitter.

Moves TOP-LEVEL (non-template, non-inline) function definitions and global variable
definitions from a header into a target .cpp, while leaving prototypes/externs behind.

- Skips: template<> blocks, inline/constexpr functions, macros, class/struct bodies,
  and constexpr *integral* constants used at compile-time.
- Preserves attributes/qualifiers textually (e.g., PROGMEM, alignas, IRAM_ATTR).
- Creates <target.cpp> if missing and inserts '#include "<header>"' once.

Usage:
  python3 scripts/split_header.py <header> <target_cpp> [--apply]
"""
import sys, re, os
from pathlib import Path

HEADER_GUARDS = re.compile(r'^\s*#\s*(ifndef|define|pragma)\b')
PP_LINE = re.compile(r'^\s*#')
TEMPLATE_LINE = re.compile(r'^\s*template\s*<')
INLINE_FCN = re.compile(r'\binline\b')
CONSTEXPR_FCN = re.compile(r'\bconstexpr\b')
SIG_HINT = re.compile(r'[A-Za-z_~]\w*\s*(::\s*[A-Za-z_]\w*)*\s*\([^;]*\)\s*(?:noexcept\s*)?(?:const\s*)?(?:->\s*[\w:<>*&\s]+)?\s*$')
IS_EXTERN = re.compile(r'^\s*extern\b')
TYPEDEF_USING = re.compile(r'^\s*(typedef|using)\b')
AGGREGATE_START = re.compile(r'^\s*(class|struct|union|enum)\b')
CONSTEXPR_INTEGRAL = re.compile(r'\bconstexpr\b(?!.*\{)')


def strip_line_comments(s):
    out = []
    in_block = False
    i = 0
    while i < len(s):
        if not in_block and i + 1 < len(s) and s[i] == '/' and s[i + 1] == '/':
            out.append('\n')
            while i < len(s) and s[i] != '\n':
                i += 1
            continue
        if not in_block and i + 1 < len(s) and s[i] == '/' and s[i + 1] == '*':
            in_block = True
            i += 2
            continue
        if in_block and i + 1 < len(s) and s[i] == '*' and s[i + 1] == '/':
            in_block = False
            i += 2
            continue
        if not in_block:
            out.append(s[i])
        i += 1
    return ''.join(out)


def find_top_level_chunks(text):
    parse = strip_line_comments(text)
    n = len(parse)
    i = 0
    depth = 0
    results = []
    in_aggregate_stack = []

    def scan_block(start):
        d = 0
        j = start
        in_str = None
        while j < n:
            c = parse[j]
            if in_str:
                if c == '\\':
                    j += 2
                    continue
                if c == in_str:
                    in_str = None
                j += 1
                continue
            if c in ('"', "'"):
                in_str = c
                j += 1
                continue
            if c == '{':
                d += 1
            elif c == '}':
                d -= 1
                if d == 0:
                    return j + 1
            j += 1
        return n

    line_starts = [0]
    for idx, ch in enumerate(parse):
        if ch == '\n':
            line_starts.append(idx + 1)

    def line_start(pos):
        last = 0
        for ls in line_starts:
            if ls <= pos:
                last = ls
            else:
                break
        return last

    while i < n:
        c = parse[i]
        if c in ('"', "'"):
            q = c
            i += 1
            while i < n:
                if parse[i] == '\\':
                    i += 2
                    continue
                if parse[i] == q:
                    i += 1
                    break
                i += 1
            continue

        if parse[i] == '\n':
            i += 1
            continue

        if parse[i] == '#':
            while i < n and parse[i] != '\n':
                i += 1
            continue

        if parse[i] != ' ' and parse[i] != '\t':
            ls = line_start(i)
            line = parse[ls: parse.find('\n', ls) if parse.find('\n', ls) != -1 else n]
            if depth == 0 and AGGREGATE_START.match(line):
                j = i
                while j < n and parse[j] != '{':
                    j += 1
                if j < n and parse[j] == '{':
                    in_aggregate_stack.append(True)
                    depth += 1
                    i = j + 1
                    continue

        if c == '{':
            depth += 1
            i += 1
            continue
        if c == '}':
            depth -= 1
            if depth < 0:
                depth = 0
            if in_aggregate_stack and depth == 0:
                in_aggregate_stack.pop()
            i += 1
            continue

        if depth == 0 and not in_aggregate_stack:
            ls = line_start(i)
            line = parse[ls: parse.find('\n', ls) if parse.find('\n', ls) != -1 else n].rstrip()

            if not line or PP_LINE.match(line) or TYPEDEF_USING.match(line):
                i = parse.find('\n', ls)
                if i == -1:
                    break
                i += 1
                continue

            looks_like_func = '(' in line and (')' in line or parse.find(')', ls) != -1)
            if not looks_like_func and '=' in parse[ls:]:
                if IS_EXTERN.match(line):
                    pass
                else:
                    j = ls
                    brace_d = 0
                    while j < n:
                        ch = parse[j]
                        if ch == '{':
                            brace_d += 1
                        elif ch == '}':
                            brace_d -= 1
                        elif ch == ';' and brace_d == 0:
                            chunk = text[ls:j + 1]
                            if CONSTEXPR_INTEGRAL.search(chunk) and '{' not in chunk:
                                pass
                            else:
                                results.append(("var", ls, j + 1, {}))
                            i = j + 1
                            break
                        j += 1
                    if j >= n:
                        break
                    continue

            if INLINE_FCN.search(line) or CONSTEXPR_FCN.search(line):
                i = parse.find('\n', ls)
                if i == -1:
                    break
                i += 1
                continue

            prev_ls = parse.rfind('\n', 0, ls)
            prev2_ls = parse.rfind('\n', 0, prev_ls) if prev_ls != -1 else -1
            prev_block = parse[prev2_ls + 1:ls]
            if TEMPLATE_LINE.search(prev_block):
                i = parse.find('\n', ls)
                if i == -1:
                    break
                i += 1
                continue

            if SIG_HINT.search(line):
                open_brace = parse.find('{', ls)
                semi_here = parse.find(';', ls, open_brace if open_brace != -1 else ls + 200)
                if open_brace != -1 and (semi_here == -1 or semi_here > open_brace):
                    end = scan_block(open_brace)
                    results.append(("func", ls, end, {}))
                    i = end
                    continue

        nxt = parse.find('\n', i)
        if nxt == -1:
            break
        i = nxt + 1

    results.sort(key=lambda x: x[1])
    coalesced = []
    last_end = -1
    for k, s, e, m in results:
        if s >= last_end:
            coalesced.append((k, s, e, m))
            last_end = e
    return coalesced


def make_prototype(def_block):
    head = def_block.split('{', 1)[0].rstrip()
    if head.endswith(')'):
        return head + ';'
    head = re.sub(r'\s+$', '', head)
    return head + ';'


def make_extern(var_block):
    m = re.search(r'^(.*?\S)\s*=\s*.*?;\s*$', var_block.strip(), re.S)
    if not m:
        return None
    left = m.group(1)
    if left.strip().startswith('extern '):
        return None
    return 'extern ' + left.strip() + ';'


def ensure_include(cpp_path, header_path):
    p = Path(cpp_path)
    text = ''
    if p.exists():
        text = p.read_text(encoding='utf-8', errors='ignore')
    inc = f'#include "{header_path}"'
    if inc not in text:
        text = inc + '\n\n' + text
    return text


def main():
    if len(sys.argv) < 3:
        print(__doc__.strip())
        sys.exit(2)
    header = Path(sys.argv[1]).as_posix()
    target = Path(sys.argv[2]).as_posix()
    do_apply = '--apply' in sys.argv

    hpath = Path(header)
    if not hpath.exists():
        print(f"ERR: header not found: {header}", file=sys.stderr)
        sys.exit(2)

    original = hpath.read_text(encoding='utf-8', errors='ignore')
    chunks = find_top_level_chunks(original)

    moved = []
    header_new_parts = []
    last = 0

    for kind, s, e, meta in chunks:
        block = original[s:e]
        if kind == 'func':
            proto = make_prototype(block)
            if proto:
                header_new_parts.append(original[last:s])
                header_new_parts.append(proto + '\n')
                moved.append(block.strip() + '\n')
                last = e
        elif kind == 'var':
            extern = make_extern(block)
            if extern:
                header_new_parts.append(original[last:s])
                header_new_parts.append(extern + '\n')
                moved.append(block.strip() + '\n')
                last = e

    header_new_parts.append(original[last:])
    header_new = ''.join(header_new_parts)

    print(f"[plan] {header}: move {len(moved)} item(s) → {target}")
    for i, m in enumerate(moved, 1):
        short = m.strip().splitlines()[0]
        suffix = '…' if len(short) > 120 else ''
        print(f"  - [{i}] {short[:120]}{suffix}")

    if not do_apply:
        print("[dry-run] No files modified. Re-run with --apply to write changes.")
        return

    bak = hpath.with_suffix(hpath.suffix + '.bak')
    bak.write_text(original, encoding='utf-8')
    hpath.write_text(header_new, encoding='utf-8')

    tpath = Path(target)
    tpath.parent.mkdir(parents=True, exist_ok=True)
    cpp_text = ensure_include(tpath, header)
    banner = f"// ===== Moved mechanically from {header}. DO NOT CHANGE SEMANTICS. =====\n\n"
    body = '\n'.join(moved)
    if tpath.exists():
        existing = tpath.read_text(encoding='utf-8', errors='ignore')
        if banner not in existing:
            cpp_text = cpp_text + banner + body + '\n' + existing
        else:
            cpp_text = existing.rstrip() + '\n\n' + body + '\n'
    else:
        cpp_text = cpp_text + banner + body + '\n'
    tpath.write_text(cpp_text, encoding='utf-8')

    print(f"[apply] Wrote {header} (backup at {bak.name}) and {target}")
    print("[next] Build & scan:")
    print("  pio run")
    print("  python3 tools/aggregate_init_scanner.py --mode=strict --roots src include lib")


if __name__ == '__main__':
    main()
