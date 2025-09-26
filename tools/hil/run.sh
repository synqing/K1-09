#!/usr/bin/env bash
set -euo pipefail

PORT="${SB_SERIAL_PORT:-}" 
if [[ -z "$PORT" ]]; then
  # try to auto-detect
  for pat in /dev/cu.usbmodem* /dev/cu.usbserial* /dev/ttyACM* /dev/ttyUSB*; do
    for dev in $pat; do
      if [[ -e "$dev" ]]; then PORT="$dev"; break; fi
    done
    [[ -n "$PORT" ]] && break
  done
fi

if [[ -z "$PORT" ]]; then
  echo "No serial port found. Set SB_SERIAL_PORT=/dev/xxx" >&2
  exit 1
fi

export SB_SERIAL_PORT="$PORT"
python -m pip install -r "$(dirname "$0")/../../tests/hil/requirements.txt"
pytest -q tests/hil "$@"

