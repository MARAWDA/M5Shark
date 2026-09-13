#!/usr/bin/env sh
set -eu

TARGET=${1:-M5SHARK_V8}
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
CONFIG="$ROOT/arduino-cli-esp32-3.3.4.yaml"
CLI=${ARDUINO_CLI:-arduino-cli}

case "$TARGET" in
  HOSYOND_35)
    DEFINE=MARAUDER_HOSYOND_35
    FQBN=esp32:esp32:esp32
    ;;
  WAVESHARE_C5_28)
    DEFINE=MARAUDER_WAVESHARE_C5_28
    FQBN='esp32:esp32:esp32c5:FlashSize=32M,PSRAM=enabled'
    ;;
  M5SHARK_V8)
    DEFINE=MARAUDER_V8
    FQBN='esp32:esp32:esp32c5:FlashSize=8M,PartitionScheme=default_8MB,PSRAM=enabled'
    ;;
  *)
    printf 'Unknown target: %s\n' "$TARGET" >&2
    exit 2
    ;;
esac

exec "$CLI" compile --config-file "$CONFIG" --clean --jobs 1 \
  --fqbn "$FQBN" --libraries "$ROOT/.build-libraries" \
  --output-dir "$ROOT/build/$(printf '%s' "$TARGET" | tr '[:upper:]' '[:lower:]')" \
  --build-property "compiler.cpp.extra_flags=-D$DEFINE" "$ROOT/m5shark"
