#!/usr/bin/env sh
set -eu

if [ "${PORT:-}" = "" ]; then
  echo "Set PORT, for example: PORT=/dev/cu.usbmodem101 scripts/upload.sh" >&2
  exit 2
fi

ARDUINO_CLI="${ARDUINO_CLI:-arduino-cli}"
ARDUINO_CONFIG="${ARDUINO_CONFIG:-arduino-cli.yaml}"
FQBN="${FQBN:-arduino:megaavr:nona4809}"
SKETCH="${SKETCH:-firmware/idum}"
BUILD_PATH="${BUILD_PATH:-build/idum}"

"$ARDUINO_CLI" --config-file "$ARDUINO_CONFIG" compile --fqbn "$FQBN" --build-path "$BUILD_PATH" "$SKETCH"
exec "$ARDUINO_CLI" --config-file "$ARDUINO_CONFIG" upload -p "$PORT" --fqbn "$FQBN" --input-dir "$BUILD_PATH" "$SKETCH"
