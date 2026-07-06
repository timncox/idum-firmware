#!/usr/bin/env sh
set -eu

ARDUINO_CLI="${ARDUINO_CLI:-arduino-cli}"
ARDUINO_CONFIG="${ARDUINO_CONFIG:-arduino-cli.yaml}"
FQBN="${FQBN:-arduino:megaavr:nona4809}"
SKETCH="${SKETCH:-firmware/idum}"
BUILD_PATH="${BUILD_PATH:-build/idum}"

exec "$ARDUINO_CLI" --config-file "$ARDUINO_CONFIG" compile --fqbn "$FQBN" --build-path "$BUILD_PATH" "$SKETCH"
