#!/usr/bin/env python3
"""
Mechanical header splitter (safe v3e).

Fixes vs v3c:
- Treat preprocessor lines as hard separators (no attempt to "finish" any pending statement on a PP line).
- PP detection is resilient even if a leading '#' is lost (matches directive keywords at BOL).
- Still: no writes if 0 moves; ignores struct/class members; detects const arrays/LUTs with multi-line brace inits.

Behavior summary:
- Moves TOP-LEVEL (non-template, non-inline) function definitions and global variable definitions from a header
  into a target .cpp; leaves prototypes/externs in the header.
- Skips: templates, inline/constexpr functions, class/struct/union/enum bodies, and constexpr integral scalars.
- Header externs drop storage/placement attrs (static, PROGMEM, alignas, __attribute__); moved definitions keep them.
"""

import os, sys, re
from pathlib import Path

# --- lexical helpers ---------------------------------------------------------

# Preprocessor line (robust): either '#' + word OR directive keyword at BOL (in case '#' was lost)
PP_OR_DIR = re.compile(
    r'^\s*(?:#\s*\w+|(?:define|undef|if|ifdef|ifndef|elif|else|endif|include|error|pragma)\b)'
)

TEMPLATE_LINE  = re.compile(r'^\s*template\s*<')
INLINE_FCN     = re.compile(r'\binline\b')
CONSTEXPR_FCN  = re.compile(r'\bconstexpr\b')
TYPEDEF_USING  = re.compile(r'^\s*(typedef|using)\b')
CLOSE_AGG_LINE = re.compile(r'^\s*}\s*;?\s*$')

# Robust aggregate starter: optional typedef/attrs/macros; '{' may be on next line
AGG_ANY = re.compile(
    r'^\s*(?:typedef\s+)?(?:__attribute__\s*\(\(.*?\)\)\s*|alignas\s*\([^)]*\)\s*|[A-Z_][A-Z0-9_]*\s+)*'
    r'(class|struct|union|enum)\b',
    re.S
)

IS_EXTERN      = re.compile(r'^\s*extern\b')
CONSTEXPR_INT  = re.compile(r'\bconstexpr\b(?!.*\{)')  # constexpr scalar (no braces)

SIG_HINT = re.compile(
    r'[A-Za-z_~]\w*\s*(::\s*[A-Za-z_]\w*)*\s*\([^;]*\)\s*(?:noexcept\s*)?(?:const\s*)?(?:->\s*[\w:<>*&\s]+)?\s*$'
)

ATTR_TOKENS = [
    r'PROGMEM', r'IRAM_ATTR', r'DRAM_ATTR', r'ICACHE_RODATA_ATTR',
    r'RTC_RODATA_ATTR', r'RODATA_ATTR'
]
ATTR_RE = re.compile(
    r'(?:\b(?:' + '|'.join(ATTR_TOKENS) + r')\b'
    r'|__attribute__\s*\(\(.*?\)\)'
    r'|alignas\s*\([^)]*\))',
    re.S
)

DECL_HEAD_RE = re.compile(
    r'^\s*(?:(?:constexpr|constinit|const|static|volatile|signed|unsigned|short|long|char|int|float|double|bool|auto'
    r'|struct|class|union)\b|[A-Za-z_]\w*(?:::\s*[A-Za-z_]\w*)*(?:\s*<[^;{}()]*>)?)'
    r'(?:\s+(?:[*&]\s*|const\b|volatile\b|constexpr\b|restrict\b))*\s+[A-Za-z_]\w*(?:\s*\[[^\]]*\])*\s*$'
)

def strip_c_comments(s: str) -> str:
    '''Remove // and /* */ while preserving length (keep '#').'''
    out = []
    i = 0
    n = len(s)
    in_block = False
    in_str = None
    while i < n:
        c = s[i]
        if in_str:
            out.append(c)
            if c == '\\\\' and i + 1 < n:
                out.append(s[i + 1])
                i += 2
                continue
            if c == in_str:
                in_str = None
            i += 1
            continue
        if not in_block and i + 1 < n and s[i] == '/' and s[i + 1] == '/':
            out.append(' ')
            out.append(' ')
            i += 2
            while i < n and s[i] != '\n':
                out.append(' ' if s[i] != '\t' else '\t')
                i += 1
            if i < n:
                out.append('\n')
                i += 1
            continue
        if not in_block and i + 1 < n and s[i] == '/' and s[i + 1] == '*':
            out.append(' ')
            out.append(' ')
            i += 2
            in_block = True
            continue
        if in_block:
            if i + 1 < n and s[i] == '*' and s[i + 1] == '/':
                out.append(' ')
                out.append(' ')
                i += 2
                in_block = False
            else:
                out.append(s[i] if s[i] == '\n' else ' ')
                i += 1
            continue
        if c in ('\"', "'"):
            in_str = c
        out.append(c)
        i += 1
    return ''.join(out)


def compact_ws(s: str) -> str:
    return re.sub(r'\s+', ' ', s).strip()

def remove_storage_attrs(s: str) -> str:
    """Strip attrs unsuitable for 'extern'."""
    s = ATTR_RE.sub(' ', s)
    s = re.sub(r'\bstatic\b', ' ', s)
    return compact_ws(s)

def normalize_decl_head(s: str) -> str:
    s = re.sub(r'(?<=\w)([*&])', r' \1', s)
    s = re.sub(r'([*&])(?=\w)', r'\1 ', s)
    return compact_ws(s)

# --- parser ------------------------------------------------------------------

def line_starts_for(text: str):
    starts = [0]
    for i,ch in enumerate(text):
        if ch == '\n': starts.append(i+1)
    return starts

def find_top_level_chunks(text: str):
    """
    Yield (kind, start_idx, end_idx, block_text) for top-level func/var defs.

    Strategy:
      - Maintain 'stmt_start' at the first non-trivial top-level line.
      - On PP lines: treat as HARD separator — reset stmt_start, skip line, DO NOT attempt to finish any pending stmt.
      - Finish statements only at a top-level ';'.
    """
    parse = strip_c_comments(text)
    n = len(parse); i = 0
    depth = 0
    in_agg = 0
    stmt_start = None
    starts = line_starts_for(parse)

    def ln_start(pos):
        last = 0
        for s in starts:
            if s <= pos: last = s
            else: break
        return last

    def lookahead_brace_before_semi(ls):
        look = parse[ls: min(n, ls + 600)]
        semi_idx = look.find(';')
        brace_idx = look.find('{')
        return brace_idx != -1 and (semi_idx == -1 or brace_idx < semi_idx), (ls + (brace_idx if brace_idx != -1 else -1))

    def scan_to_semicolon(pos):
        d = 0; j = pos
        while j < n:
            c = parse[j]
            if c == '"':
                j += 1
                while j < n and (parse[j] != '"' or parse[j-1] == '\\'): j += 1
            elif c == "'":
                j += 1
                while j < n and (parse[j] != "'" or parse[j-1] == '\\'): j += 1
            else:
                if c == '{': d += 1
                elif c == '}':
                    d -= 1
                    if d < 0: d = 0
                elif c == ';' and d == 0:
                    return j+1
            j += 1
        return -1

    while i < n:
        ls = ln_start(i)
        line = parse[ls: parse.find('\n', ls) if parse.find('\n', ls) != -1 else n].rstrip()
        c = parse[i] if i < n else '\n'

        # Preprocessor line: HARD separator (no finishing pending stmt)
        if PP_OR_DIR.match(line):
            stmt_start = None
            nxt = parse.find('\n', i)
            if nxt == -1: break
            i = nxt + 1
            continue

        # Brace tracking (outside PP)
        if c == '{':
            depth += 1; i += 1; continue
        if c == '}':
            depth = max(0, depth-1)
            if in_agg and depth == 0:
                in_agg -= 1
            i += 1; continue

        # Aggregate start at top level
        if depth == 0 and AGG_ANY.search(line):
            brace_before_semi, brace_pos = lookahead_brace_before_semi(ls)
            if brace_before_semi:
                in_agg += 1
                depth += 1
                i = (brace_pos + 1) if brace_pos != -1 else i + 1
                stmt_start = None
                continue

        if depth == 0 and in_agg == 0:
            if CLOSE_AGG_LINE.match(line):
                stmt_start = None
                nxt = parse.find('\n', i)
                if nxt == -1:
                    break
                i = nxt + 1
                continue
            # Trivial skipper lines (typedef/using or blank)
            if TYPEDEF_USING.match(line) or not line.strip():
                # Do not try to close a statement here; just move on
                nxt = parse.find('\n', i)
                if nxt == -1: break
                i = nxt + 1
                continue

            # Anchor a new statement if needed
            if stmt_start is None:
                stmt_start = ls

            # Try to finish at next top-level ';'
            end_stmt = scan_to_semicolon(i)
            if end_stmt != -1:
                yield from _classify_statement(text, parse, stmt_start, end_stmt)
                stmt_start = None
                i = end_stmt
                continue

        # default advance
        nxt = parse.find('\n', i)
        if nxt == -1: break
        i = nxt + 1

def _classify_statement(original_text, parse_text, start_idx, end_idx):
    """Classify a top-level statement; yield ('func'/'var', start, end, block)."""
    stmt = parse_text[start_idx:end_idx]

    # If the slice contains any preprocessor line, refuse classification.
    for ln in stmt.splitlines():
        if PP_OR_DIR.match(ln):
            return

    # Skip extern-only
    if IS_EXTERN.match(stmt.strip()):
        return

    # Skip templates (check a small look-behind window)
    back = parse_text[max(0, start_idx-200):start_idx]
    if TEMPLATE_LINE.search(back):
        return

    # Skip inline/constexpr functions
    head_line = stmt.splitlines()[0] if stmt.splitlines() else stmt
    if INLINE_FCN.search(head_line) or CONSTEXPR_FCN.search(head_line):
        return

    # Function definition? signature line with '{' before ';'
    if SIG_HINT.search(head_line):
        open_brace = stmt.find('{')
        semi_here  = stmt.find(';', 0, open_brace if open_brace != -1 else 0)
        if open_brace != -1 and (semi_here == -1 or semi_here > open_brace):
            block = original_text[start_idx:end_idx]
            yield ("func", start_idx, end_idx, block)
            return

    # Variable definition (arrays, big initializers, or explicit '=')
    has_equals      = '=' in stmt
    has_braces_init = '{' in stmt and '}' in stmt
    is_array_decl   = '[' in stmt and ']' in stmt

    # constexpr integral scalars (no braces) should stay in header
    if CONSTEXPR_INT.search(stmt) and not has_braces_init and not is_array_decl:
        return

    looks_like_func = '(' in stmt and ')' in stmt and '{' not in stmt[:stmt.find(')')+1]
    if (has_equals or (is_array_decl and has_braces_init)) and not looks_like_func:
        init_pos = top_level_init_pos(stmt)
        if init_pos == -1:
            return
        left = remove_storage_attrs(stmt[:init_pos])
        left_clean = left.strip()
        if not left_clean or '.' in left_clean or '->' in left_clean:
            return
        if not DECL_HEAD_RE.match(normalize_decl_head(left_clean)):
            return
        block = original_text[start_idx:end_idx]
        yield ("var", start_idx, end_idx, block)
        return

# --- transforms --------------------------------------------------------------

NAME_ARRAYS_RE = re.compile(
    r'^(?P<prefix>.*\b)(?P<name>[A-Za-z_]\w*)\s*(?P<arr>(?:\s*\[[^\]]*\])*)\s*(?P<trail>.*)$',
    re.S
)

def make_prototype(def_block: str) -> str:
    head = def_block.split('{', 1)[0].rstrip()
    if head.endswith(')'): return head + ';'
    return re.sub(r'\s+$','',head) + ';'

def top_level_init_pos(block: str) -> int:
    """Return index of '=' at top-level, or first '{' after declarator for brace-init without '='; else -1."""
    txt = strip_c_comments(block)
    d_b = d_p = 0
    i = 0; n = len(txt)
    first_brace = -1
    while i < n:
        c = txt[i]
        if c == '"':
            i += 1
            while i < n and (txt[i] != '"' or txt[i-1] == '\\'): i += 1
        elif c == "'":
            i += 1
            while i < n and (txt[i] != "'" or txt[i-1] == '\\'): i += 1
        else:
            if c == '{': d_b += 1
            elif c == '}': d_b -= 1
            elif c == '(': d_p += 1
            elif c == ')': d_p -= 1
            elif c == '=' and d_b == d_p == 0:
                return i
            if c == '{' and d_p == 0 and d_b == 1 and first_brace == -1:
                first_brace = i
        i += 1
    return first_brace

def make_extern(var_block: str):
    """
    Create 'extern …;' from a var definition (keeps type/const/arrays, strips storage attrs).
    Returns extern line or None (e.g., constexpr).
    """
    if re.search(r'\bconstexpr\b', var_block):
        return None

    init_pos = top_level_init_pos(var_block)
    if init_pos == -1:
        return None

    left = remove_storage_attrs(var_block[:init_pos])
    left_clean = left.strip()

    if not left_clean or '.' in left_clean or '->' in left_clean:
        return None
    if not DECL_HEAD_RE.match(normalize_decl_head(left_clean)):
        return None

    m = NAME_ARRAYS_RE.match(left_clean)
    if not m:
        return None

    prefix = compact_ws(m.group('prefix'))
    name   = m.group('name')
    arrays = compact_ws(m.group('arr') or '')

    if not name:
        return None

    decl = f"extern {prefix} {name}{arrays};" if prefix else f"extern {name}{arrays};"
    return compact_ws(decl)

def ensure_include_once(cpp_text: str, header_path: str, target_path: str) -> str:
    header_posix = Path(header_path).as_posix()
    target_dir = Path(target_path).parent
    try:
        rel = Path(os.path.relpath(header_posix, target_dir)).as_posix()
    except ValueError:
        rel = header_posix
    inc = f'#include "{rel}"'
    if inc not in cpp_text:
        cpp_text = inc + '\n\n' + cpp_text
    return cpp_text


# --- main --------------------------------------------------------------------

def main():
    if len(sys.argv) < 3:
        print(__doc__.strip()); sys.exit(2)
    header = Path(sys.argv[1]).as_posix()
    target = Path(sys.argv[2]).as_posix()
    do_apply = '--apply' in sys.argv

    hpath = Path(header)
    if not hpath.exists():
        print(f"ERR: header not found: {header}", file=sys.stderr); sys.exit(2)

    original = hpath.read_text(encoding='utf-8', errors='ignore')

    moved_blocks = []
    header_new_parts = []
    last = 0
    for kind, s, e, block in find_top_level_chunks(original):
        if kind == 'func':
            proto = make_prototype(block)
            if proto:
                header_new_parts.append(original[last:s]); header_new_parts.append(proto + '\n')
                moved_blocks.append(block.strip() + '\n'); last = e
        elif kind == 'var':
            extern = make_extern(block)
            if extern:
                header_new_parts.append(original[last:s]); header_new_parts.append(extern + '\n')
                moved_blocks.append(block.strip() + '\n'); last = e

    header_new_parts.append(original[last:])
    header_new = ''.join(header_new_parts)
    move_count = len(moved_blocks)

    print(f"[plan] {header}: move {move_count} item(s) → {target}")
    for i, m in enumerate(moved_blocks, 1):
        first = m.strip().splitlines()[0]
        print(f"  - [{i}] {first[:120]}{'…' if len(first)>120 else ''}")

    if move_count == 0:
        print("[plan] No moves detected. Exiting without changes.")
        sys.exit(0)

    if not do_apply:
        print("[dry-run] No files modified. Re-run with --apply to write changes.")
        return

    # Write header (only if changed)
    if header_new != original:
        bak = hpath.with_suffix(hpath.suffix + '.bak')
        bak.write_text(original, encoding='utf-8')
        hpath.write_text(header_new, encoding='utf-8')
        print(f"[apply] Wrote {header} (backup at {bak.name})")
    else:
        print("[apply] Header text unchanged (safe).")

    # Write/append cpp
    tpath = Path(target); tpath.parent.mkdir(parents=True, exist_ok=True)
    existing = tpath.read_text(encoding='utf-8', errors='ignore') if tpath.exists() else ''
    cpp_text = ensure_include_once(existing, header, target)
    banner = f"// ===== Moved mechanically from {header}. DO NOT CHANGE SEMANTICS. =====\n"
    if banner not in cpp_text:
        cpp_text = banner + "\n" + cpp_text
    for block in moved_blocks:
        if block not in cpp_text:
            cpp_text = cpp_text.rstrip() + "\n\n" + block
    tpath.write_text(cpp_text, encoding='utf-8')
    print(f"[apply] Updated {target} with {move_count} block(s).")
    print("[next] Verify:")
    print("  pio run")
    print("  python3 tools/aggregate_init_scanner.py --mode=strict --roots src include lib")

if __name__ == '__main__':
    main()
