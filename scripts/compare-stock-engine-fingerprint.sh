#!/usr/bin/env bash
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
stock_sim="$repo_root/build/sim/idum-sim-none"
alternate_sim="$repo_root/build/sim/idum-sim-superlooper"

stock_fingerprint=$($stock_sim stock-fingerprint)
alternate_fingerprint=$($alternate_sim stock-fingerprint)

if [[ "$stock_fingerprint" != "$alternate_fingerprint" ]]; then
  echo "Stock engine parity failed." >&2
  echo "stock:       $stock_fingerprint" >&2
  echo "superlooper: $alternate_fingerprint" >&2
  exit 1
fi

echo "PASS stock-engine-parity: $stock_fingerprint"
