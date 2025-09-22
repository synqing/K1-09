#!/usr/bin/env bash
set -Eeuo pipefail
./scripts/split_header.py include/performance_optimized_trace.h src/trace/performance_optimized_trace.cpp "$@"
