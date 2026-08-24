#!/usr/bin/env sh
set -eu

ALT_ENGINE="${ALT_ENGINE:-superlooper}"
STOCK_SKETCH="${STOCK_SKETCH:-firmware/idum}"
ENGINE_SOURCE="${ENGINE_SOURCE:-firmware/engines/$ALT_ENGINE}"
STAGED_SKETCH="${STAGED_SKETCH:-build/staged/$ALT_ENGINE/idum}"

if [ ! -d "$STOCK_SKETCH" ]; then
  echo "Stock/shared sketch source not found: $STOCK_SKETCH" >&2
  exit 2
fi

case "$STAGED_SKETCH" in
  build/staged/*/idum) ;;
  *)
    echo "Refusing to replace staging path outside build/staged/*/idum: $STAGED_SKETCH" >&2
    exit 2
    ;;
esac

if [ "$ALT_ENGINE" != "none" ] && [ ! -d "$ENGINE_SOURCE" ]; then
  echo "Engine source not found: $ENGINE_SOURCE" >&2
  exit 2
fi

rm -rf "$STAGED_SKETCH"
mkdir -p "$STAGED_SKETCH"
cp -R "$STOCK_SKETCH/." "$STAGED_SKETCH/"

if [ "$ALT_ENGINE" != "none" ]; then
  cp -R "$ENGINE_SOURCE/." "$STAGED_SKETCH/"
fi

printf 'Staged %s sketch: %s\n' "$ALT_ENGINE" "$STAGED_SKETCH"
