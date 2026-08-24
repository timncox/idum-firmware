#!/usr/bin/env sh
set -eu

ARDUINO_CLI="${ARDUINO_CLI:-arduino-cli}"
ARDUINO_CONFIG="${ARDUINO_CONFIG:-arduino-cli.yaml}"
FQBN="${FQBN:-arduino:megaavr:nona4809:mode=off}"
ALT_ENGINE="${ALT_ENGINE:-superlooper}"
STOCK_SKETCH="${STOCK_SKETCH:-firmware/idum}"
ENGINE_SOURCE="${ENGINE_SOURCE:-firmware/engines/$ALT_ENGINE}"
STAGED_SKETCH="${STAGED_SKETCH:-build/staged/$ALT_ENGINE/idum}"
BUILD_PATH="${BUILD_PATH:-build/idum-$ALT_ENGINE}"
FIRMWARE_BASENAME="${FIRMWARE_BASENAME:-alt-dum-$ALT_ENGINE}"
FIRMWARE_VERSION="${FIRMWARE_VERSION:-1.0.0}"
FIRMWARE_DEFINES="${FIRMWARE_DEFINES:-}"
RELEASE_DIR="${RELEASE_DIR:-build/release}"

case "$ALT_ENGINE" in
  superlooper) engine_id=1 ;;
  none)
    echo "ALT_ENGINE=none is simulator/reference-only" >&2
    exit 2
    ;;
  *)
    echo "Unknown ALT_ENGINE: $ALT_ENGINE" >&2
    exit 2
    ;;
esac

ALT_ENGINE="$ALT_ENGINE" STOCK_SKETCH="$STOCK_SKETCH" ENGINE_SOURCE="$ENGINE_SOURCE" STAGED_SKETCH="$STAGED_SKETCH" scripts/stage-sketch.sh

"$ARDUINO_CLI" --config-file "$ARDUINO_CONFIG" compile --fqbn "$FQBN" --build-path "$BUILD_PATH" --build-property "compiler.cpp.extra_flags=-DIDUM_ALT_ENGINE=$engine_id $FIRMWARE_DEFINES" "$STAGED_SKETCH"
mkdir -p "$RELEASE_DIR"
cp "$BUILD_PATH/idum.ino.hex" "$RELEASE_DIR/$FIRMWARE_BASENAME-v$FIRMWARE_VERSION.hex"
printf 'Release firmware: %s\n' "$RELEASE_DIR/$FIRMWARE_BASENAME-v$FIRMWARE_VERSION.hex"
