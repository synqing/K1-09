#!/usr/bin/env python3
"""
Pick 5 headers that appear most in include cycles and open 'header-split' issues.

Requires:
  - analysis/deps_report.json (from tools/deps_stub_report.py)
  - GitHub CLI: gh auth status == logged in
Usage:
  python3 scripts/open_header_split_issues.py --dry-run
  python3 scripts/open_header_split_issues.py
"""
import argparse
import json
import shutil
import subprocess
from collections import Counter
from pathlib import Path

ISSUE_TEMPLATE = """\
**Goal**
Mechanical split of `{hdr}` into declaration header and implementation source, with zero behaviour change.

**Files**
- Header (keep declarations only): `{hdr}`
- New implementation file (create): `{cpp}`

**Rules (MANDATORY)**
1. Do **not** introduce new functions or variables in firmware.
2. Do **not** change signatures or logic.
3. Move **definitions** from `{hdr}` into `{cpp}`.
4. Apply IWYU (include tightening; prefer forward decls).
5. Respect aggregate-init policy (strict) — scanner must pass.
6. `pio run` must succeed.

**Checklist**
- [ ] Declarations remain in `{hdr}`
- [ ] Definitions moved to `{cpp}`
- [ ] Includes minimized / forward decls used
- [ ] Build OK (`pio run`)
- [ ] Scanner OK (`python3 tools/aggregate_init_scanner.py --mode=strict --roots src include lib`)
- [ ] Dependency report regenerated (`python3 tools/deps_stub_report.py src include lib`)
"""


def load_report():
    path = Path("analysis/deps_report.json")
    if not path.exists():
        raise SystemExit("Missing analysis/deps_report.json. Run deps_stub_report.py first.")
    return json.loads(path.read_text())


def choose_top_headers(report, count):
    freq = Counter()
    for cycle in report.get("cycles", []):
        for file_id in cycle[:-1]:
            freq[file_id] += 1
    candidates = [
        f
        for f, _ in freq.most_common()
        if f.endswith((".h", ".hpp", ".hh", ".ipp")) and Path(f).exists()
    ]
    scored = []
    for file_id in candidates:
        score = freq[file_id]
        if file_id.startswith("include/"):
            score *= 2
        scored.append((score, file_id))
    scored.sort(reverse=True)
    if scored:
        return [file_id for _, file_id in scored[:count]]

    # Fallback: use highest in-degree headers when no cycles present.
    incoming = Counter()
    for edge in report.get("edges", []):
        target = edge["to"]
        if target.endswith((".h", ".hpp", ".hh", ".ipp")) and Path(target).exists():
            incoming[target] += 1
    ranked = []
    for header, score in incoming.most_common():
        boost = 2 if header.startswith("include/") else 1
        ranked.append((score * boost, header))
    ranked.sort(reverse=True)
    return [header for _, header in ranked[:count]]


def default_cpp_path(header: str) -> str:
    header_path = Path(header)
    if header_path.parts and header_path.parts[0] == "include":
        return str(Path("src") / header_path.relative_to("include").with_suffix(".cpp"))
    return str(header_path.with_suffix(".cpp"))


def create_issue(header: str, dry_run: bool):
    cpp_path = default_cpp_path(header)
    title = f"header-split: {header} → {header} + {cpp_path}"
    body = ISSUE_TEMPLATE.format(hdr=header, cpp=cpp_path)
    labels = ["header-split", "mechanical", "safe-change"]
    if dry_run:
        print("\n--- DRY RUN ---")
        print("TITLE:", title)
        print("BODY:\n", body)
        print("LABELS:", ", ".join(labels))
        return
    cmd = [
        "gh",
        "issue",
        "create",
        "--title",
        title,
        "--body",
        body,
        "--label",
        ",".join(labels),
    ]
    subprocess.check_call(cmd)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--count", type=int, default=5)
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    report = load_report()
    top_headers = choose_top_headers(report, args.count)
    if not top_headers:
        raise SystemExit("No headers found in cycles. Ensure the dependency report lists cycles.")

    if not args.dry_run and shutil.which("gh") is None:
        raise SystemExit("GitHub CLI (gh) not found. Install or run with --dry-run.")

    for header in top_headers:
        create_issue(header, args.dry_run)


if __name__ == "__main__":
    main()
