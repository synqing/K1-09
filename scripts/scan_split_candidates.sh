#!/usr/bin/env bash
set -Eeuo pipefail; IFS=$'
	'
# scan_split_candidates.sh: list headers and how many items would be moved by the splitter.
# Output is sorted by move count (desc).

need() { command -v "$1" >/dev/null 2>&1 || { echo "Missing dependency: $1" >&2; exit 2; }; }
need python3
need sed
need git

headers=$(git ls-files | grep -E '^(src|include)/.*\.h$' | sort)
if [[ -z "$headers" ]]; then
  echo "No headers under src/ or include/"; exit 0
fi

printf "%5s  %-56s  ->  %s
" "MOVES" "HEADER" "TARGET"
while IFS= read -r h; do
  [[ -z "$h" ]] && continue
  base="$(basename "$h")"; name="${base%.h}"
  target="src/core/${name}.cpp"
  plan="$(python3 scripts/split_header.py "$h" "$target" || true)"
  moves="$(printf "%s
" "$plan" | sed -n -E 's/.*move ([0-9]+) item\(s\).*/\1/p' | tail -n1)"
  [[ -z "$moves" ]] && moves=0
  printf "%5d  %-56s  ->  %s
" "$moves" "$h" "$target"
done <<< "$headers" | sort -nr -k1,1
