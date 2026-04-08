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

run_sql_and_assert() {
  local sql_content="$1"
  local expected_stdout="$2"
  local expected_stderr="$3"
  local expected_code="$4"

  local tmp
  tmp="$(mktemp -d)"
  trap 'rm -rf "$tmp"' RETURN

  write_required_schemas "$tmp"
  printf "%s" "$sql_content" >"$tmp/input.sql"

  local out_file="$tmp/stdout.txt"
  local err_file="$tmp/stderr.txt"
  local expected_out_file="$tmp/expected_stdout.txt"
  local expected_err_file="$tmp/expected_stderr.txt"
  printf "%s" "$expected_stdout" >"$expected_out_file"
  printf "%s" "$expected_stderr" >"$expected_err_file"

  set +e
  (
    cd "$tmp"
    "$BIN" "input.sql" >"$out_file" 2>"$err_file"
  )
  local code=$?
  set -e

  [ "$code" -eq "$expected_code" ] || return 1
  diff -u "$expected_out_file" "$out_file" >/dev/null
  diff -u "$expected_err_file" "$err_file" >/dev/null
}

case_all_statements_success_exit_zero() {
  local sql
  sql=$'INSERT INTO users (name, age, major) VALUES (\'김민준\', 25, \'컴퓨터공학\');\nSELECT name FROM users;\n'
  local expected_out
  expected_out=$'name\n김민준\n'
  local expected_err
  expected_err=''
  run_sql_and_assert "$sql" "$expected_out" "$expected_err" 0
}

case_any_error_makes_exit_one() {
  local sql
  sql=$'INSERT INTO users (name, age) VALUES (\'김민준\', 25);\nSELECT name FROM users;\n'
  local expected_out
  expected_out=''
  local expected_err
  expected_err=$'ERROR: column count does not match value count\n'
  run_sql_and_assert "$sql" "$expected_out" "$expected_err" 1
}

case_mixed_errors_keep_running_and_exit_one() {
  local sql
  sql=$'INSERT INTO users (name, age, major) VALUES (\'김민준\', 25, \'컴퓨터공학\');\nINSERT INTO users (name, age) VALUES (\'이서연\', 22);\nSELECT * FROM orders;\nINSERT INTO users (name, age, major) VALUES (\'박지호\', 23, \'물리학\');\nSELECT name FROM users;\n'
  local expected_out
  expected_out=$'name\n김민준\n박지호\n'
  local expected_err
  expected_err=$'ERROR: column count does not match value count\nERROR: table not found\n'
  run_sql_and_assert "$sql" "$expected_out" "$expected_err" 1
}

case_one_error_message_per_statement() {
  local sql
  sql=$'INSERT INTO users (name, age) VALUES (\'김민준\');\nSELECT name FROM users;\n'
  local expected_out
  expected_out=''
  local expected_err
  expected_err=$'ERROR: column count does not match value count\n'
  run_sql_and_assert "$sql" "$expected_out" "$expected_err" 1
}

run_case "exit_all_statements_success_exit_zero" case_all_statements_success_exit_zero
run_case "exit_any_error_makes_exit_one" case_any_error_makes_exit_one
run_case "exit_mixed_errors_keep_running_and_exit_one" case_mixed_errors_keep_running_and_exit_one
run_case "exit_one_error_message_per_statement" case_one_error_message_per_statement

echo ""
echo "Total: $TOTAL, Pass: $PASS, Fail: $FAIL"

[ "$FAIL" -eq 0 ]
