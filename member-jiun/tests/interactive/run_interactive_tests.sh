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

strip_ansi() {
  perl -pe 's/\e\[[0-9;]*[A-Za-z]//g'
}

run_interactive_and_capture() {
  local dir="$1"
  local input_text="$2"
  local out_file="$3"
  local err_file="$4"

  set +e
  (
    cd "$dir"
    printf "%s" "$input_text" | "$BIN" >"$out_file" 2>"$err_file"
  )
  local code=$?
  set -e
  return "$code"
}

case_interactive_help_command() {
  tmp="$(mktemp -d)"
  trap 'rm -rf "$tmp"' RETURN
  write_required_schemas "$tmp"

  local out_file="$tmp/stdout.txt"
  local err_file="$tmp/stderr.txt"
  run_interactive_and_capture "$tmp" $'.help\n.quit\n' "$out_file" "$err_file"
  local code=$?

  local actual_out
  actual_out="$(strip_ansi <"$out_file")"

  [ "$code" -eq 0 ] || return 1
  [ ! -s "$err_file" ] || return 1
  printf '%s' "$actual_out" | grep -F "Available commands" >/dev/null
  printf '%s' "$actual_out" | grep -F ".tables" >/dev/null
  printf '%s' "$actual_out" | grep -F ".schema <table>" >/dev/null
}

case_interactive_tables_command() {
  tmp="$(mktemp -d)"
  trap 'rm -rf "$tmp"' RETURN
  write_required_schemas "$tmp"

  local out_file="$tmp/stdout.txt"
  local err_file="$tmp/stderr.txt"
  run_interactive_and_capture "$tmp" $'.tables\n.quit\n' "$out_file" "$err_file"
  local code=$?

  local actual_out
  actual_out="$(strip_ansi <"$out_file")"

  [ "$code" -eq 0 ] || return 1
  [ ! -s "$err_file" ] || return 1
  printf '%s' "$actual_out" | grep -F "Available tables:" >/dev/null
  printf '%s' "$actual_out" | grep -F "users" >/dev/null
  printf '%s' "$actual_out" | grep -F "products" >/dev/null
}

case_interactive_schema_command() {
  tmp="$(mktemp -d)"
  trap 'rm -rf "$tmp"' RETURN
  write_required_schemas "$tmp"

  local out_file="$tmp/stdout.txt"
  local err_file="$tmp/stderr.txt"
  run_interactive_and_capture "$tmp" $'.schema users\n.quit\n' "$out_file" "$err_file"
  local code=$?

  local actual_out
  actual_out="$(strip_ansi <"$out_file")"

  [ "$code" -eq 0 ] || return 1
  [ ! -s "$err_file" ] || return 1
  printf '%s' "$actual_out" | grep -F "Schema for users" >/dev/null
  printf '%s' "$actual_out" | grep -F "name: string" >/dev/null
  printf '%s' "$actual_out" | grep -F "age: int" >/dev/null
}

case_interactive_sql_execution() {
  tmp="$(mktemp -d)"
  trap 'rm -rf "$tmp"' RETURN
  write_required_schemas "$tmp"

  local out_file="$tmp/stdout.txt"
  local err_file="$tmp/stderr.txt"
  run_interactive_and_capture "$tmp" $'INSERT INTO users (name, age, major) VALUES (\'nako\', 30, \'computer science\');\nSELECT name, age FROM users;\n.quit\n' "$out_file" "$err_file"
  local code=$?

  local actual_out
  actual_out="$(strip_ansi <"$out_file")"

  [ "$code" -eq 0 ] || return 1
  [ ! -s "$err_file" ] || return 1
  printf '%s' "$actual_out" | grep -F "1 row inserted." >/dev/null
  printf '%s' "$actual_out" | grep -F "name,age" >/dev/null
  printf '%s' "$actual_out" | grep -F "nako,30" >/dev/null
  printf '%s' "$actual_out" | grep -F "(1 row)" >/dev/null
}

run_case "interactive_help_command" case_interactive_help_command
run_case "interactive_tables_command" case_interactive_tables_command
run_case "interactive_schema_command" case_interactive_schema_command
run_case "interactive_sql_execution" case_interactive_sql_execution

echo ""
echo "Total: $TOTAL, Pass: $PASS, Fail: $FAIL"

[ "$FAIL" -eq 0 ]
