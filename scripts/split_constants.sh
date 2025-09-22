#!/usr/bin/env bash
set -Eeuo pipefail
./scripts/split_header.py src/constants.h src/core/constants.cpp "$@"
