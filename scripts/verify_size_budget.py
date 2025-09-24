#!/usr/bin/env python3
"""Verify firmware size against the stored baseline."""
from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", default="ci/size-baseline.json",
                        help="JSON file containing baseline flash/ram budget")
    parser.add_argument("--pio", default="pio",
                        help="PlatformIO executable (default: %(default)s)")
    return parser.parse_args()


def load_baseline(path: Path) -> dict:
    with path.open() as fh:
        data = json.load(fh)
    required = {"flash_bytes", "ram_bytes", "tolerance_percent"}
    missing = required.difference(data)
    if missing:
        raise ValueError(f"baseline missing keys: {', '.join(sorted(missing))}")
    return data


def run_size(pio_exe: str) -> tuple[int, int]:
    try:
        result = subprocess.run(
            [pio_exe, "run", "-t", "size"],
            check=True,
            capture_output=True,
            text=True,
        )
    except subprocess.CalledProcessError as exc:
        sys.stderr.write(exc.stderr)
        raise SystemExit(exc.returncode)

    lines = result.stdout.strip().splitlines()
    table_lines = [line for line in lines if line.strip().startswith(".") or line.strip().startswith("text")]
    # Expect the last line to contain numbers, format: text data bss dec hex filename
    for line in reversed(lines):
        parts = line.split()
        if len(parts) >= 6 and parts[0].isdigit():
            text, data, bss = (int(parts[i]) for i in range(3))
            flash = text + data
            ram = data + bss
            return flash, ram
    raise RuntimeError("failed to parse size output; ensure 'pio run -t size' format unchanged")


def main() -> int:
    args = parse_args()
    baseline_file = Path(args.baseline)

    if not baseline_file.exists():
        print(f"Baseline file missing: {baseline_file}", file=sys.stderr)
        return 1

    baseline = load_baseline(baseline_file)
    flash_curr, ram_curr = run_size(args.pio)

    tol = baseline["tolerance_percent"] / 100.0
    flash_limit = baseline["flash_bytes"] * (1.0 + tol)
    ram_limit = baseline["ram_bytes"] * (1.0 + tol)

    print(f"Flash: {flash_curr} bytes (limit {flash_limit:.0f})")
    print(f"RAM  : {ram_curr} bytes (limit {ram_limit:.0f})")

    ok = True
    if flash_curr > flash_limit:
        print(f"ERROR: flash usage exceeds limit by {flash_curr - flash_limit:.0f} bytes", file=sys.stderr)
        ok = False
    if ram_curr > ram_limit:
        print(f"ERROR: RAM usage exceeds limit by {ram_curr - ram_limit:.0f} bytes", file=sys.stderr)
        ok = False

    return 0 if ok else 2


if __name__ == "__main__":
    sys.exit(main())
