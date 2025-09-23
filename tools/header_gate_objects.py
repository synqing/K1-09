#!/usr/bin/env python3
"""
header_gate_objects.py
Guard top-level OBJECT definitions in a header with a macro, turning them into
  - extern declarations when MACRO is *not* defined
  - original definitions when MACRO *is* defined

Patterns handled (simple but effective for project style):
  1) <qualifiers> <type> <name> = <init> ;
  2) <qualifiers> <type> <name>[...]= { ... } ;
  3) <qualifiers> <type> <name> { ... } ;    (aggregate init)

Will NOT touch lines already containing 'extern' or a preprocessor directive.

USAGE:
  python3 tools/header_gate_objects.py <header.h> <MACRO> <name1> <name2> ...

Exit:
  0 = success (changed or no-op), 2 = usage/header missing, 3 = none of the names found
"""
import sys,re
from pathlib import Path

DECL_RE = re.compile(
    r'^(?P<prefix>(?:\s*(?:constexpr|const|static|volatile|alignas\([^)]*\)|__attribute__\(\(.*?\)\)|[A-Z_][A-Z0-9_]*))*\s*'
    r'(?:[A-Za-z_]\w*(?:::\s*[A-Za-z_]\w*)*(?:\s*<[^;{}()]*>)?'
    r'(?:\s+[*&])*\s+))'
    r'(?P<name>[A-Za-z_]\w*)'
    r'(?P<arr>(?:\s*\[[^\]]*\])*)\s*'
    r'(?P<init>\=\s*[^;{][^;]*|=\s*\{[^;]*\}|\s*\{[^;]*\})\s*;',
    re.S
)

def guard_object(src:str, target:str, macro:str):
    out_lines=[]
    found=False
    for line in src.splitlines(True):
        if 'extern' in line or line.lstrip().startswith('#'):
            out_lines.append(line); continue
        m=DECL_RE.match(line)
        if m and m.group('name')==target:
            found=True
            prefix=m.group('prefix').strip()
            name=m.group('name')
            arr =m.group('arr') or ''
            extern=f"extern {prefix} {name}{arr};".replace('  ',' ').replace(' ;',';')
            guarded=(
                f"#ifndef {macro}\n"
                f"{extern}\n"
                f"#else\n"
                f"{line.rstrip()}\n"
                f"#endif\n"
            )
            out_lines.append(guarded)
        else:
            out_lines.append(line)
    return ''.join(out_lines),found

def main():
    if len(sys.argv)<4:
        print(__doc__); sys.exit(2)
    hdr=Path(sys.argv[1]); macro=sys.argv[2]; names=sys.argv[3:]
    if not hdr.exists(): print(f"ERR: header not found: {hdr}", file=sys.stderr); sys.exit(2)
    s=hdr.read_text(encoding='utf-8', errors='ignore')
    any_found=False
    for n in names:
        s,found=guard_object(s,n,macro); any_found|=found
    if not any_found:
        print("WARNING: none of the requested objects were found", file=sys.stderr); sys.exit(3)
    bak=hdr.with_suffix(hdr.suffix+'.bak'); bak.write_text(hdr.read_text(encoding='utf-8', errors='ignore'), encoding='utf-8')
    hdr.write_text(s, encoding='utf-8')
    print(f"[gate-obj] Wrote {hdr} (backup {bak.name}); guarded {', '.join(names)}.")
if __name__=='__main__':
    main()
