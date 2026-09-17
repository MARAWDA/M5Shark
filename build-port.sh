#!/usr/bin/env sh
set -eu

TARGET=${1:-M5SHARK_V8}
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
CONFIG="$ROOT/arduino-cli-esp32-3.3.4.yaml"
CLI=${ARDUINO_CLI:-arduino-cli}

case "$TARGET" in
  HOSYOND_35)
    DEFINE=MARAUDER_HOSYOND_35
    FQBN='esp32:esp32:esp32:PartitionScheme=custom'
    EXTRA_FLAGS='-DMARAUDER_HOSYOND_35 -DMARAUDER_V8'
    ;;
  WAVESHARE_C5_28)
    DEFINE=MARAUDER_WAVESHARE_C5_28
    FQBN='esp32:esp32:esp32c5:FlashSize=32M,PSRAM=enabled'
    EXTRA_FLAGS="-D$DEFINE"
    ;;
  M5SHARK_V8)
    DEFINE=MARAUDER_V8
    FQBN='esp32:esp32:esp32c5:FlashSize=8M,PartitionScheme=default_8MB,PSRAM=enabled'
    EXTRA_FLAGS="-D$DEFINE"
    ;;
  *)
    printf 'Unknown target: %s\n' "$TARGET" >&2
    exit 2
    ;;
esac

TOOLCHAIN_BIN=$(find "$ROOT/C:/Users/itsom/.arduino334/packages/esp32/tools" \
  -type f -name 'xtensa-esp32-elf-g++' -printf '%h\n' -quit 2>/dev/null || true)
if [ -n "$TOOLCHAIN_BIN" ]; then
  TOOLCHAIN_LINK="$ROOT/.xtensa-toolchain-bin"
  rm -f "$TOOLCHAIN_LINK"
  ln -sfn "$TOOLCHAIN_BIN" "$TOOLCHAIN_LINK"
  if [ ! -e "$TOOLCHAIN_LINK/ld" ] && [ -e "$TOOLCHAIN_LINK/xtensa-esp32-elf-ld" ]; then
    ln -s "xtensa-esp32-elf-ld" "$TOOLCHAIN_LINK/ld"
  fi
  export PATH="$TOOLCHAIN_LINK:$PATH"
fi

exec "$CLI" compile --config-file "$CONFIG" --jobs "${ARDUINO_JOBS:-$(nproc)}" \
  --fqbn "$FQBN" --libraries "$ROOT/.build-libraries" \
  --output-dir "$ROOT/build/$(printf '%s' "$TARGET" | tr '[:upper:]' '[:lower:]')" \
  --build-property "compiler.cpp.extra_flags=$EXTRA_FLAGS" "$ROOT/m5shark"
