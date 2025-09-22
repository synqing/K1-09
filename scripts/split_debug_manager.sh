#!/usr/bin/env bash
set -Eeuo pipefail
./scripts/split_header.py src/debug/debug_manager.h src/debug/debug_manager.cpp "$@"
