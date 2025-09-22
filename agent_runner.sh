#!/usr/bin/env bash
# POSIX-ish fail-fast wrapper for any agent step.
set -Eeuo pipefail
IFS=$'\n\t'
log() { printf '>> %s\n' "$*" >&2; }

trap 'rc=$?; log "FAIL: $BASH_COMMAND (exit $rc)"; exit $rc' ERR

log "CMD: $*"
"$@"
rc=$?
log "EXIT: $rc"
exit "$rc"
