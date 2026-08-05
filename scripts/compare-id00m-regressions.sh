#!/usr/bin/env bash

set -u

repo_dir=$(cd "$(dirname "$0")/.." && pwd)
id00m_root=${1:-${ID00M_ROOT:-}}

if [[ -z "$id00m_root" ]]; then
  echo "Usage: $0 /path/to/id00m-source-root" >&2
  echo "The root must contain upstream/IDUM_Firmware_v_99 and firmware/IDUM_Custom." >&2
  exit 2
fi

stock_source="$id00m_root/upstream/IDUM_Firmware_v_99"
id00m_source="$id00m_root/firmware/IDUM_Custom"

if [[ ! -d "$stock_source" || ! -d "$id00m_source" ]]; then
  echo "Could not find the stock and ID00M source directories under: $id00m_root" >&2
  exit 2
fi

stage_dir=$(mktemp -d "${TMPDIR:-/tmp}/idum-regressions.XXXXXX")
trap 'rm -rf "$stage_dir"' EXIT

mkdir -p "$stage_dir/stock" "$stage_dir/id00m"
cp -R "$stock_source/." "$stage_dir/stock/"
cp -R "$id00m_source/." "$stage_dir/id00m/"

# The firmware's AVR-only fast I/O header cannot compile on a desktop host.
# Overlay our behavior-neutral simulator shim without modifying either source tree.
cp "$repo_dir/firmware/idum/fastdigitalIO.h" "$stage_dir/stock/fastdigitalIO.h"
cp "$repo_dir/firmware/idum/fastdigitalIO.h" "$stage_dir/id00m/fastdigitalIO.h"

if ! make -C "$repo_dir" sim SIM_BUILD=build/sim/idum-current >"$stage_dir/current-build.log" 2>&1; then
  cat "$stage_dir/current-build.log" >&2
  exit 1
fi

if ! make -C "$repo_dir" sim \
  SIM_BUILD=build/sim/idum-stock \
  SIM_FIRMWARE="$stage_dir/stock" \
  SIM_MAIN=IDUM_Firmware_v_99.ino \
  SIM_DEFINES= >"$stage_dir/stock-build.log" 2>&1; then
  cat "$stage_dir/stock-build.log" >&2
  exit 1
fi

if ! make -C "$repo_dir" sim \
  SIM_BUILD=build/sim/id00m \
  SIM_FIRMWARE="$stage_dir/id00m" \
  SIM_MAIN=IDUM_Custom.ino \
  SIM_DEFINES='-DIDUM_SIM_ID00M=1 -DIDUM_SIM_CONST_BREAK_TABLE=1' >"$stage_dir/id00m-build.log" 2>&1; then
  cat "$stage_dir/id00m-build.log" >&2
  exit 1
fi

scenarios=(
  regression-length-1
  regression-rotate-split-param
  regression-rotate-silent-destination
  regression-ball-channel-3-reset
  regression-back-to-back-mode
  regression-removed-mode-boot
  regression-split-loop-empty-step
  regression-loop-phase
  regression-loop-ratchet-map
  regression-ratchet-zero-interval
  regression-rng-power-on
)

run_variant() {
  local label=$1
  local binary=$2
  local require_fixed=${3:-0}
  local failures=0
  local scenario output status

  echo
  echo "$label"
  for scenario in "${scenarios[@]}"; do
    output=$("$binary" "$scenario" 2>&1)
    status=$?
    if [[ -n "$output" ]]; then
      echo "$output" | tail -n 1
    elif [[ $status -eq 136 ]]; then
      echo "CRASH ratchet-zero-interval: integer divide/modulo by zero (SIGFPE)"
    else
      echo "ERROR $scenario: simulator exited $status"
    fi

    if [[ $require_fixed -eq 1 && $status -ne 0 ]]; then
      failures=$((failures + 1))
    fi
  done

  return "$failures"
}

ulimit -c 0
run_variant "Stock v.99" "$repo_dir/build/sim/idum-stock"
if ! run_variant "Our current fork" "$repo_dir/build/sim/idum-current" 1; then
  echo "Our fork failed one or more faithful-fix regressions." >&2
  exit 1
fi
run_variant "ID00M v1.0.3" "$repo_dir/build/sim/id00m"
