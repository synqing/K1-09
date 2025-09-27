#!/usr/bin/env python3
import pathlib
import re

rx = re.compile(r'^[ \t]*([A-Za-z_][\w:<>,\[\]\*& \t]+?[ \t]+([A-Za-z_]\w*))\s*\([^;{}]*\)\s*(?:noexcept\s*)?(?:const\s*)?(?:->[^\{]*)?\s*\{', re.MULTILINE)
roots = ["src", "include"]
results = {}
for root in roots:
    for path in pathlib.Path(root).rglob("*.h"):
        try:
            text = path.read_text(encoding="utf-8", errors="ignore")
        except OSError:
            continue
        names = []
        for match in rx.finditer(text):
            name = match.group(2)
            if len(name) == 0:
                continue
            if name not in names:
                names.append(name)
        if names:
            results[path.as_posix()] = names

for header in sorted(results):
    print(f"{header} => {', '.join(results[header])}")
