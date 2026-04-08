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

case_select_star_outputs_header_and_rows() {
  local sql
  sql=$'INSERT INTO users (name, age, major) VALUES (\'김민준\', 25, \'컴퓨터공학\');\nINSERT INTO users (name, age, major) VALUES (\'이서연\', 22, \'경영학\');\nSELECT * FROM users;\n'
  local expected_out
  expected_out=$'name,age,major\n김민준,25,컴퓨터공학\n이서연,22,경영학\n'
  local expected_err
  expected_err=''
  run_sql_and_assert "$sql" "$expected_out" "$expected_err" 0
}

case_select_specific_columns_in_requested_order() {
  local sql
  sql=$'INSERT INTO users (name, age, major) VALUES (\'김민준\', 25, \'컴퓨터공학\');\nSELECT major, name FROM users;\n'
  local expected_out
  expected_out=$'major,name\n컴퓨터공학,김민준\n'
  local expected_err
  expected_err=''
  run_sql_and_assert "$sql" "$expected_out" "$expected_err" 0
}

case_select_empty_table_is_silent() {
  local sql
  sql=$'SELECT * FROM users;\n'
  local expected_out
  expected_out=''
  local expected_err
  expected_err=''
  run_sql_and_assert "$sql" "$expected_out" "$expected_err" 0
}

case_select_invalid_column_reports_invalid_query() {
  local sql
  sql=$'INSERT INTO users (name, age, major) VALUES (\'김민준\', 25, \'컴퓨터공학\');\nSELECT name, email FROM users;\n'
  local expected_out
  expected_out=''
  local expected_err
  expected_err=$'ERROR: invalid query\n'
  run_sql_and_assert "$sql" "$expected_out" "$expected_err" 1
}

case_select_table_not_found() {
  local sql
  sql=$'SELECT * FROM orders;\n'
  local expected_out
  expected_out=''
  local expected_err
  expected_err=$'ERROR: table not found\n'
  run_sql_and_assert "$sql" "$expected_out" "$expected_err" 1
}

case_select_error_does_not_stop_next_statement() {
  local sql
  sql=$'SELECT name, email FROM users;\nINSERT INTO users (name, age, major) VALUES (\'박지호\', 23, \'물리학\');\nSELECT name FROM users;\n'
  local expected_out
  expected_out=$'name\n박지호\n'
  local expected_err
  expected_err=$'ERROR: invalid query\n'
  run_sql_and_assert "$sql" "$expected_out" "$expected_err" 1
}

run_case "select_star_outputs_header_and_rows" case_select_star_outputs_header_and_rows
run_case "select_specific_columns_in_requested_order" case_select_specific_columns_in_requested_order
run_case "select_empty_table_is_silent" case_select_empty_table_is_silent
run_case "select_invalid_column_reports_invalid_query" case_select_invalid_column_reports_invalid_query
run_case "select_table_not_found" case_select_table_not_found
run_case "select_error_does_not_stop_next_statement" case_select_error_does_not_stop_next_statement

echo ""
echo "Total: $TOTAL, Pass: $PASS, Fail: $FAIL"

[ "$FAIL" -eq 0 ]
