#!/usr/bin/env python3
"""
Include graph stub -> Mermaid + JSON + DOT.

Emits to analysis/:
  - deps_mermaid.md   (human)
  - deps_report.json  (machine)
  - deps_graph.dot    (graphviz)
"""
import collections
import json
import pathlib
import re
import sys

INC = re.compile(r'^\s*#\s*include\s*(["<])([^">]+)[">]')


def resolve_include(token, delim, current_path, root_paths, project_root):
    if delim == '<':
        return None

    candidate = (current_path.parent / token).resolve()
    if candidate.exists():
        return candidate

    for root in root_paths:
        candidate = (root / token).resolve()
        if candidate.exists():
            return candidate
    return None


def collect(roots):
    project_root = pathlib.Path('.').resolve()
    root_paths = []
    for root in roots:
        root_path = (project_root / root).resolve() if not pathlib.Path(root).is_absolute() else pathlib.Path(root)
        if not root_path.exists():
            raise SystemExit(f"[deps] Missing root: {root}")
        root_paths.append(root_path)

    files = []
    edges = collections.defaultdict(set)
    for root_path in root_paths:
        for path in root_path.rglob('*'):
            if path.suffix.lower() not in {'.h', '.hpp', '.hh', '.ipp', '.cpp', '.cc', '.cxx', '.ino'}:
                continue
            if not path.is_file():
                continue
            try:
                rel = str(path.relative_to(project_root))
            except ValueError:
                rel = str(path)
            files.append(rel)

            try:
                for line in path.read_text(encoding='utf-8', errors='ignore').splitlines():
                    match = INC.match(line)
                    if not match:
                        continue
                    delim, token = match.groups()
                    resolved = resolve_include(token, delim, path, root_paths, project_root)
                    if resolved is not None:
                        try:
                            target = str(resolved.relative_to(project_root))
                        except ValueError:
                            target = str(resolved)
                    else:
                        target = token
                    edges[rel].add(target)
            except Exception:
                pass

    return sorted(set(files)), {k: sorted(v) for k, v in edges.items()}


def find_cycles(edges):
    graph = {src: set(dsts) for src, dsts in edges.items()}
    cycles, seen = [], set()

    def dfs(start, node, path, visited):
        for nxt in graph.get(node, []):
            if nxt == start:
                cycle = path + [nxt]
                key = tuple(cycle)
                if key not in seen:
                    seen.add(key)
                    cycles.append(cycle)
            elif nxt not in visited and len(path) < 200:
                dfs(start, nxt, path + [nxt], visited | {nxt})

    for start in graph:
        dfs(start, start, [start], {start})

    normalised = []
    for cycle in cycles:
        min_idx = min(range(len(cycle) - 1), key=lambda i: cycle[i])
        rotated = cycle[min_idx:-1] + cycle[:min_idx] + [cycle[min_idx]]
        if rotated not in normalised:
            normalised.append(rotated)
    return normalised


def mermaid(edges):
    lines = ["```mermaid", "graph LR"]
    emitted = set()
    for src, dsts in edges.items():
        for dst in dsts:
            key = (src, dst)
            if key in emitted:
                continue
            emitted.add(key)
            lines.append(f'  "{src}" --> "{dst}"')
    lines.append("```")
    return "\n".join(lines)


def dot(edges):
    lines = ["digraph G {", "  rankdir=LR;"]
    emitted = set()
    for src, dsts in edges.items():
        for dst in dsts:
            key = (src, dst)
            if key in emitted:
                continue
            emitted.add(key)
            safe_src = src.replace('"', '')
            safe_dst = dst.replace('"', '')
            lines.append(f'  "{safe_src}" -> "{safe_dst}";')
    lines.append("}")
    return "\n".join(lines)


if __name__ == "__main__":
    roots = sys.argv[1:] or ["src", "include", "lib"]
    files, edges = collect(roots)
    cycles = find_cycles(edges)

    analysis_dir = pathlib.Path("analysis")
    analysis_dir.mkdir(exist_ok=True)

    mermaid_path = analysis_dir / "deps_mermaid.md"
    json_path = analysis_dir / "deps_report.json"
    dot_path = analysis_dir / "deps_graph.dot"

    mermaid_path.write_text(mermaid(edges), encoding="utf-8")
    report = {
        "files": files,
        "edges": [
            {"from": src, "to": dst}
            for src, dsts in edges.items()
            for dst in dsts
        ],
        "cycles": cycles,
        "odr_conflicts": [],
    }
    json_path.write_text(json.dumps(report, indent=2), encoding="utf-8")
    dot_path.write_text(dot(edges), encoding="utf-8")

    print("Wrote: analysis/deps_mermaid.md, deps_report.json, deps_graph.dot")
