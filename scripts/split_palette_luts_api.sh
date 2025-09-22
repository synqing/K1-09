#!/usr/bin/env bash
set -Eeuo pipefail
./scripts/split_header.py src/palettes/palette_luts_api.h src/palettes/palette_luts_api.cpp "$@"
