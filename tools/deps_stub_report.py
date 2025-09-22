#!/usr/bin/env python3
"""Dependency graph + ODR risk analysis for Sensory Bridge firmware.

Outputs three artefacts by default:
- Mermaid diagram (Markdown) for quick inspection
- GraphViz DOT file for high-fidelity visualisation
- JSON report containing cycles, ODR findings, and per-file metrics

Heuristics are intentionally conservative: we only flag symbols defined at
syntactic depth 0 (top-level) and treat quoted includes as project-local.
"""
from __future__ import annotations

import argparse
import json
import re
from collections import defaultdict
from dataclasses import dataclass, asdict
from pathlib import Path
from typing import Dict, Iterable, List, Optional, Set, Tuple

import networkx as nx

INCLUDE_RE = re.compile(r'^\s*#\s*include\s*["<]([^">]+)[">]')
# Skip typedef/using/enum struct lines; capture type + identifier for globals
GLOBAL_RE = re.compile(
    r'^\s*(?!extern)(?!static)(?!template)(?!using)(?!typedef)(?!struct)(?!class)'
    r'([\w:<>~]+(?:\s+[\w:<>\*&]+)*)\s+(\w+)\s*(?:\[.*?\])?\s*=')
FUNCTION_RE = re.compile(
    r'^\s*(?:inline\s+)?(?:static\s+)?(?:constexpr\s+)?([\w:<>~]+(?:\s+[\w:<>\*&]+)*)\s+(\w+)\s*\([^;]*\)\s*(?:noexcept\s*)?\{')
COMMENT_RE = re.compile(r'//.*$')
MULTILINE_COMMENT_RE = re.compile(r'/\*.*?\*/', re.DOTALL)

SOURCE_EXTS = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".ino"}

@dataclass
class FileMetrics:
    path: str
    includes: List[str]
    symbols: List[str]
    functions: List[str]
    incoming: int = 0
    outgoing: int = 0
    risk_score: int = 0

    def compute_risk(self) -> None:
        # Simple heuristic: fan-in + fan-out + symbol count weighted
        self.outgoing = len(self.includes)
        self.risk_score = self.incoming + self.outgoing + len(self.symbols)


def iter_source_files(roots: Iterable[str]) -> Iterable[Path]:
    for root in roots:
        base = Path(root)
        if not base.exists():
            continue
        for path in base.rglob('*'):
            if path.suffix.lower() in SOURCE_EXTS and path.is_file():
                yield path.resolve()


def normalise_path(path: Path, project_root: Path) -> str:
    try:
        return str(path.relative_to(project_root))
    except ValueError:
        return str(path)


def strip_comments(content: str) -> str:
    """Remove single-line and multi-line comments to avoid brace drift."""
    content = MULTILINE_COMMENT_RE.sub('', content)
    lines = []
    for line in content.splitlines():
        lines.append(COMMENT_RE.sub('', line))
    return '\n'.join(lines)


def analyse_file(path: Path, project_root: Path) -> FileMetrics:
    text = path.read_text(errors='ignore')
    stripped = strip_comments(text)
    includes: List[str] = []
    symbols: List[str] = []
    functions: List[str] = []

    brace_depth = 0

    for raw_line in stripped.splitlines():
        line = raw_line.strip()
        include_match = INCLUDE_RE.match(raw_line)
        if include_match:
            includes.append(include_match.group(1))
            # Still allow brace tracking for include lines

        if brace_depth == 0:
            g_match = GLOBAL_RE.match(raw_line)
            if g_match:
                symbols.append(g_match.group(2))
            else:
                f_match = FUNCTION_RE.match(raw_line)
                if f_match:
                    functions.append(f_match.group(2))

        brace_depth += raw_line.count('{') - raw_line.count('}')
        if brace_depth < 0:
            brace_depth = 0

    metrics = FileMetrics(
        path=normalise_path(path, project_root),
        includes=includes,
        symbols=symbols,
        functions=functions,
    )
    return metrics


def resolve_include(include: str, current_file: Path, search_roots: List[Path]) -> Optional[Path]:
    """Resolve a quoted include to an actual file within the project."""
    if include.startswith('<') or include.endswith('>'):
        return None

    candidate = (current_file.parent / include).resolve()
    if candidate.exists():
        return candidate

    for root in search_roots:
        candidate = (root / include).resolve()
        if candidate.exists():
            return candidate
    return None


def build_graph(metrics_map: Dict[str, FileMetrics], project_root: Path, search_roots: List[Path]) -> nx.DiGraph:
    graph = nx.DiGraph()
    for file_id, metrics in metrics_map.items():
        graph.add_node(file_id)

    for file_id, metrics in metrics_map.items():
        src_path = project_root / file_id
        for include in metrics.includes:
            resolved = resolve_include(include, src_path, search_roots)
            if resolved:
                target = normalise_path(resolved, project_root)
                graph.add_edge(file_id, target)
            else:
                # External include: keep as dotted node for completeness
                ext_node = f"<external>: {include}"
                graph.add_node(ext_node)
                graph.add_edge(file_id, ext_node)

    for node in graph.nodes:
        if node in metrics_map:
            metrics_map[node].incoming = graph.in_degree(node)
            metrics_map[node].compute_risk()
    return graph


def find_odr_violations(metrics_map: Dict[str, FileMetrics]) -> Dict[str, List[str]]:
    symbol_locations: Dict[str, List[str]] = defaultdict(list)
    for file_id, metrics in metrics_map.items():
        for symbol in metrics.symbols:
            symbol_locations[symbol].append(file_id)
    odr = {sym: files for sym, files in symbol_locations.items() if len(files) > 1}
    return odr


def detect_cycles(graph: nx.DiGraph) -> List[List[str]]:
    scc = list(nx.strongly_connected_components(graph))
    cycles: List[List[str]] = []
    for component in scc:
        if len(component) > 1:
            cycles.append(sorted(component))
    return cycles


def write_mermaid(graph: nx.DiGraph, dest: Path) -> None:
    lines = ["```mermaid", "graph LR"]
    emitted: Set[Tuple[str, str]] = set()
    for src, dst in graph.edges:
        key = (src, dst)
        if key in emitted:
            continue
        emitted.add(key)
        lines.append(f'  "{src}" --> "{dst}"')
    lines.append("```")
    dest.write_text('\n'.join(lines))


def write_dot(graph: nx.DiGraph, dest: Path) -> None:
    lines = ["digraph deps {"]
    for node in graph.nodes:
        safe = node.replace('"', '\"')
        if node.startswith('<external>'):
            lines.append(f'  "{safe}" [shape=box, style=dashed, color=gray];')
        else:
            lines.append(f'  "{safe}";')
    for src, dst in graph.edges:
        lines.append(f'  "{src}" -> "{dst}";')
    lines.append('}')
    dest.write_text('\n'.join(lines))


def write_json(report: Dict, dest: Path) -> None:
    dest.write_text(json.dumps(report, indent=2, sort_keys=True))


def main() -> None:
    parser = argparse.ArgumentParser(description="Generate dependency graph artefacts")
    parser.add_argument('--roots', nargs='+', default=['src', 'include'])
    parser.add_argument('--json', default='deps_report.json')
    parser.add_argument('--mermaid', default='deps_mermaid.md')
    parser.add_argument('--dot', default='deps_graph.dot')
    args = parser.parse_args()

    project_root = Path('.').resolve()
    roots = [Path(r).resolve() for r in args.roots]

    metrics_map: Dict[str, FileMetrics] = {}
    for path in iter_source_files(args.roots):
        metrics = analyse_file(path, project_root)
        metrics_map[metrics.path] = metrics

    graph = build_graph(metrics_map, project_root, roots)
    cycles = detect_cycles(graph)
    odr = find_odr_violations(metrics_map)

    report = {
        'cycles': cycles,
        'odr_conflicts': odr,
        'files': {
            file_id: asdict(metrics)
            for file_id, metrics in sorted(metrics_map.items())
        },
    }

    analysis_dir = Path('analysis')
    analysis_dir.mkdir(exist_ok=True)

    write_json(report, analysis_dir / args.json)
    write_mermaid(graph, analysis_dir / args.mermaid)
    write_dot(graph, analysis_dir / args.dot)

    print(f"wrote {analysis_dir/args.json}")
    print(f"wrote {analysis_dir/args.mermaid}")
    print(f"wrote {analysis_dir/args.dot}")


if __name__ == '__main__':
    main()
