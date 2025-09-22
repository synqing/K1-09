#!/usr/bin/env python3
"""
Very light include graph (stub) -> Mermaid output for PRs.
Replace later with the full libclang analyzer per architecture doc.
"""
import collections
import pathlib
import re
import sys

INC = re.compile(r'^\s*#\s*include\s*["<]([^">]+)[">]')


def collect(roots):
    edges = collections.defaultdict(set)
    for root in roots:
        for path in pathlib.Path(root).rglob("*.*"):
            if path.suffix.lower() not in {".h", ".hpp", ".hh", ".ipp", ".cpp", ".cc", ".cxx", ".ino"}:
                continue
            try:
                for line in path.read_text(errors="ignore").splitlines():
                    match = INC.match(line)
                    if match:
                        edges[str(path.relative_to('.'))].add(match.group(1))
            except Exception:
                pass
    return edges


def mermaid(edges):
    output = ["```mermaid", "graph LR"]
    seen = set()
    for source, targets in edges.items():
        source = source.replace('"', '')
        for target in targets:
            key = (source, target)
            if key in seen:
                continue
            seen.add(key)
            output.append(f'  "{source}" --> "{target}"')
    output.append("```")
    return "\n".join(output)


if __name__ == "__main__":
    roots = sys.argv[1:] or ["src", "include"]
    edges = collect(roots)
    pathlib.Path("analysis").mkdir(exist_ok=True)
    print(mermaid(edges))
