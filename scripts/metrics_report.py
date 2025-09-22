#!/usr/bin/env python3
"""Minimal repo metrics helper.

Counts headers (include/ *.h,*.hpp,*.hh,*.ipp), sources (src/ *.cpp,*.cc,*.cxx),
and, if analysis/deps_report.json exists, reports edge and cycle counts.

Optional --write-analysis runs the dependency stub tool first.
"""
from __future__ import annotations

import argparse
import json
import os
import subprocess
from pathlib import Path
from typing import Iterable

HEADER_EXTS = {".h", ".hpp", ".hh", ".ipp"}
SOURCE_EXTS = {".cpp", ".cc", ".cxx"}


def count_files(root: Path, exts: Iterable[str]) -> int:
    total = 0
    if not root.exists():
        return 0
    for dirpath, _, filenames in os.walk(root):
        for name in filenames:
            if Path(name).suffix in exts:
                total += 1
    return total


def run_dep_tool(roots: list[str]) -> None:
    tool = Path("tools/deps_stub_report.py")
    if not tool.exists():
        return
    cmd = ["python3", str(tool), *roots]
    try:
        subprocess.run(cmd, check=False)
    except Exception:
        pass


def read_dep_report() -> dict | None:
    path = Path("analysis/deps_report.json")
    if not path.exists():
        return None
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except Exception:
        return None


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--roots", nargs="+", default=["src", "include", "lib"], help="Roots to feed to deps tool")
    parser.add_argument("--write-analysis", action="store_true", help="Regenerate dependency report before collecting metrics")
    args = parser.parse_args()

    if args.write_analysis:
        run_dep_tool(list(args.roots))

    headers = count_files(Path("include"), HEADER_EXTS)
    sources = count_files(Path("src"), SOURCE_EXTS)

    edges = cycles = None
    report = read_dep_report()
    if report:
        edges = len(report.get("edges", []))
        cycles = len(report.get("cycles", []))

    payload = {
        "headers_count": headers,
        "sources_count": sources,
        "edges_count": edges,
        "cycles_count": cycles,
    }
    print(json.dumps(payload, indent=2))


if __name__ == "__main__":
    main()
