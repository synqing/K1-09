#!/usr/bin/env bash
set -Eeuo pipefail
./scripts/split_header.py src/globals.h src/core/globals.cpp "$@"
