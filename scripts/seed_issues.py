#!/usr/bin/env python3
"""
Seed GitHub issues from a simple JSON file.

Input format:
{
  "issues": [
    {
      "title": "Title",
      "labels": ["label1", "label2"],
      "milestone": "Milestone name",
      "paths": ["path/one", "path/two"],
      "acceptance_checks": ["cmd1", "cmd2"],
      "notes": "Optional notes"
    }
  ]
}

Usage:
  python scripts/seed_issues.py --file docs/05.tracking/phase-4-issues.json --dry-run
  python scripts/seed_issues.py --file docs/05.tracking/phase-5-issues.json --run

Requires: GitHub CLI (`gh`) authenticated.
"""
from __future__ import annotations

import argparse
import json
import shlex
import subprocess
from pathlib import Path


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--file", required=True, help="Path to issues JSON")
    p.add_argument("--run", action="store_true", help="Execute gh commands (otherwise print)")
    return p.parse_args()


def load_issues(path: Path) -> list[dict]:
    data = json.loads(path.read_text())
    return data.get("issues", [])


def build_body(issue: dict) -> str:
    lines = []
    if issue.get("notes"):
        lines.append(issue["notes"])
        lines.append("")
    if issue.get("paths"):
        lines.append("Paths:")
        for p in issue["paths"]:
            lines.append(f"- `{p}`")
        lines.append("")
    if issue.get("acceptance_checks"):
        lines.append("Acceptance checks:")
        for c in issue["acceptance_checks"]:
            lines.append(f"- `{c}`")
        lines.append("")
    return "\n".join(lines).strip()


def run_or_print(cmd: list[str], do_run: bool) -> int:
    if not do_run:
        print("$", " ".join(shlex.quote(c) for c in cmd))
        return 0
    return subprocess.call(cmd)


def main() -> int:
    args = parse_args()
    path = Path(args.file)
    issues = load_issues(path)
    for issue in issues:
        title = issue.get("title", "(no title)")
        labels = ",".join(issue.get("labels", []))
        milestone = issue.get("milestone")
        body = build_body(issue)
        cmd = ["gh", "issue", "create", "--title", title]
        if labels:
            cmd += ["--label", labels]
        if milestone:
            cmd += ["--milestone", milestone]
        if body:
            cmd += ["--body", body]
        rc = run_or_print(cmd, args.run)
        if rc != 0:
            return rc
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

