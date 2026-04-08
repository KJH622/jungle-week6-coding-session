#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD_DIR="$ROOT_DIR/tests/unit/build"
TEST_BIN="$BUILD_DIR/test_core"

mkdir -p "$BUILD_DIR"

cc -std=c11 -Wall -Wextra -Werror -O2 \
  -o "$TEST_BIN" \
  "$ROOT_DIR/tests/unit/test_core.c" \
  "$ROOT_DIR/src/domain/parser.c" \
  "$ROOT_DIR/src/domain/schema.c" \
  "$ROOT_DIR/src/infrastructure/file_io.c" \
  "$ROOT_DIR/src/shared/text.c" \
  "$ROOT_DIR/src/shared/strvec.c" \
  "$ROOT_DIR/src/shared/error.c"

"$TEST_BIN"
