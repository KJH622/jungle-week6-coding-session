#!/usr/bin/env bash
set -euo pipefail

if [ $# -lt 1 ]; then
  echo "Usage: $0 <sql_processor_path>"
  exit 1
fi

BIN_INPUT="$1"
BIN="$(cd "$(dirname "$BIN_INPUT")" && pwd)/$(basename "$BIN_INPUT")"

if [ ! -x "$BIN" ]; then
  echo "[ERROR] executable not found or not executable: $BIN"
  exit 1
fi

TOTAL=0
PASS=0
FAIL=0

run_case() {
  local name="$1"
  shift

  TOTAL=$((TOTAL + 1))
  if "$@"; then
    echo "[PASS] $name"
    PASS=$((PASS + 1))
  else
    echo "[FAIL] $name"
    FAIL=$((FAIL + 1))
  fi
}

write_required_schemas() {
  local dir="$1"

  cat >"$dir/users.schema" <<'SCHEMA_USERS'
{
  "table": "users",
  "columns": [
    { "name": "name", "type": "string" },
    { "name": "age", "type": "int" },
    { "name": "major", "type": "string" }
  ]
}
SCHEMA_USERS

  cat >"$dir/products.schema" <<'SCHEMA_PRODUCTS'
{
  "table": "products",
  "columns": [
    { "name": "name", "type": "string" },
    { "name": "price", "type": "int" },
    { "name": "category", "type": "string" }
  ]
}
SCHEMA_PRODUCTS
}

case_init_with_required_schema_success() {
  tmp="$(mktemp -d)"
  trap 'rm -rf "$tmp"' RETURN

  write_required_schemas "$tmp"

  cat >"$tmp/input.sql" <<'EOF_SQL'
SELECT * FROM users;
EOF_SQL

  local out_file="$tmp/stdout.txt"
  local err_file="$tmp/stderr.txt"

  set +e
  (
    cd "$tmp"
    "$BIN" "input.sql" >"$out_file" 2>"$err_file"
  )
  local code=$?
  set -e

  [ "$code" -eq 0 ] || return 1
  [ ! -s "$out_file" ] || return 1
  [ ! -s "$err_file" ] || return 1

  return 0
}

case_init_missing_users_schema_should_fail() {
  tmp="$(mktemp -d)"
  trap 'rm -rf "$tmp"' RETURN

  cat >"$tmp/products.schema" <<'SCHEMA_PRODUCTS'
{
  "table": "products",
  "columns": [
    { "name": "name", "type": "string" },
    { "name": "price", "type": "int" },
    { "name": "category", "type": "string" }
  ]
}
SCHEMA_PRODUCTS

  cat >"$tmp/input.sql" <<'EOF_SQL'
SELECT * FROM users;
EOF_SQL

  local out_file="$tmp/stdout.txt"
  local err_file="$tmp/stderr.txt"

  set +e
  (
    cd "$tmp"
    "$BIN" "input.sql" >"$out_file" 2>"$err_file"
  )
  local code=$?
  set -e

  [ "$code" -ne 0 ] || return 1
  [ ! -s "$out_file" ] || return 1
  [ -s "$err_file" ] || return 1

  return 0
}

case_init_missing_products_schema_should_fail() {
  tmp="$(mktemp -d)"
  trap 'rm -rf "$tmp"' RETURN

  cat >"$tmp/users.schema" <<'SCHEMA_USERS'
{
  "table": "users",
  "columns": [
    { "name": "name", "type": "string" },
    { "name": "age", "type": "int" },
    { "name": "major", "type": "string" }
  ]
}
SCHEMA_USERS

  cat >"$tmp/input.sql" <<'EOF_SQL'
SELECT * FROM users;
EOF_SQL

  local out_file="$tmp/stdout.txt"
  local err_file="$tmp/stderr.txt"

  set +e
  (
    cd "$tmp"
    "$BIN" "input.sql" >"$out_file" 2>"$err_file"
  )
  local code=$?
  set -e

  [ "$code" -ne 0 ] || return 1
  [ ! -s "$out_file" ] || return 1
  [ -s "$err_file" ] || return 1

  return 0
}

case_init_malformed_schema_should_fail() {
  tmp="$(mktemp -d)"
  trap 'rm -rf "$tmp"' RETURN

  cat >"$tmp/users.schema" <<'SCHEMA_USERS'
{ invalid json
SCHEMA_USERS

  cat >"$tmp/products.schema" <<'SCHEMA_PRODUCTS'
{
  "table": "products",
  "columns": [
    { "name": "name", "type": "string" },
    { "name": "price", "type": "int" },
    { "name": "category", "type": "string" }
  ]
}
SCHEMA_PRODUCTS

  cat >"$tmp/input.sql" <<'EOF_SQL'
SELECT * FROM users;
EOF_SQL

  local out_file="$tmp/stdout.txt"
  local err_file="$tmp/stderr.txt"

  set +e
  (
    cd "$tmp"
    "$BIN" "input.sql" >"$out_file" 2>"$err_file"
  )
  local code=$?
  set -e

  [ "$code" -ne 0 ] || return 1
  [ ! -s "$out_file" ] || return 1
  [ -s "$err_file" ] || return 1

  return 0
}

case_init_unreadable_schema_should_fail() {
  tmp="$(mktemp -d)"
  trap 'chmod 644 "$tmp/users.schema" 2>/dev/null || true; rm -rf "$tmp"' RETURN

  write_required_schemas "$tmp"
  chmod 000 "$tmp/users.schema"

  cat >"$tmp/input.sql" <<'EOF_SQL'
SELECT * FROM users;
EOF_SQL

  local out_file="$tmp/stdout.txt"
  local err_file="$tmp/stderr.txt"

  set +e
  (
    cd "$tmp"
    "$BIN" "input.sql" >"$out_file" 2>"$err_file"
  )
  local code=$?
  set -e

  [ "$code" -ne 0 ] || return 1
  [ ! -s "$out_file" ] || return 1
  [ -s "$err_file" ] || return 1

  return 0
}

run_case "init_with_required_schema_success" case_init_with_required_schema_success
run_case "init_missing_users_schema_should_fail" case_init_missing_users_schema_should_fail
run_case "init_missing_products_schema_should_fail" case_init_missing_products_schema_should_fail
run_case "init_malformed_schema_should_fail" case_init_malformed_schema_should_fail
run_case "init_unreadable_schema_should_fail" case_init_unreadable_schema_should_fail

echo ""
echo "Total: $TOTAL, Pass: $PASS, Fail: $FAIL"

[ "$FAIL" -eq 0 ]
