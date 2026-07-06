#!/usr/bin/env sh
set -eu

ARDUINO_CLI="${ARDUINO_CLI:-arduino-cli}"

if ! command -v "$ARDUINO_CLI" >/dev/null 2>&1; then
  cat >&2 <<'EOF'
arduino-cli was not found.

Install it with one of these methods:
  brew install arduino-cli
  https://arduino.github.io/arduino-cli/latest/installation/

Then run:
  make setup
  make build
EOF
  exit 1
fi

"$ARDUINO_CLI" version

