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

  local tmp
  tmp="$(mktemp -d)"
  trap 'rm -rf "$tmp"' RETURN

  write_required_schemas "$tmp"
  printf "%s" "$sql_content" >"$tmp/input.sql"

  local out_file="$tmp/stdout.txt"
  local err_file="$tmp/stderr.txt"
  local expected_file="$tmp/expected_stdout.txt"
  printf "%s" "$expected_stdout" >"$expected_file"

  set +e
  (
    cd "$tmp"
    "$BIN" "input.sql" >"$out_file" 2>"$err_file"
  )
  local code=$?
  set -e

  [ "$code" -eq 0 ] || return 1
  [ ! -s "$err_file" ] || return 1
  diff -u "$expected_file" "$out_file" >/dev/null
}

case_blank_lines_are_ignored() {
  local sql
  sql=$'INSERT INTO users (name, age, major) VALUES (\'김민준\', 25, \'컴퓨터공학\');\n\nINSERT INTO users (name, age, major) VALUES (\'이서연\', 22, \'경영학\');\n\n\nSELECT * FROM users;\n'
  local expected
  expected=$'name,age,major\n김민준,25,컴퓨터공학\n이서연,22,경영학\n'
  run_sql_and_assert "$sql" "$expected"
}

case_whitespace_around_statements_is_ignored() {
  local sql
  sql=$'   INSERT INTO users (name, age, major) VALUES (\'김민준\', 25, \'컴퓨터공학\');   \n\tSELECT name, major FROM users;   \n'
  local expected
  expected=$'name,major\n김민준,컴퓨터공학\n'
  run_sql_and_assert "$sql" "$expected"
}

case_multiple_statements_in_one_line() {
  local sql
  sql=$'INSERT INTO users (name, age, major) VALUES (\'김민준\', 25, \'컴퓨터공학\'); INSERT INTO users (name, age, major) VALUES (\'박지호\', 23, \'물리학\'); SELECT name, age FROM users;\n'
  local expected
  expected=$'name,age\n김민준,25\n박지호,23\n'
  run_sql_and_assert "$sql" "$expected"
}

case_last_statement_without_trailing_semicolon() {
  local sql
  sql=$'INSERT INTO users (name, age, major) VALUES (\'김민준\', 25, \'컴퓨터공학\');\nSELECT name, age FROM users'
  local expected
  expected=$'name,age\n김민준,25\n'
  run_sql_and_assert "$sql" "$expected"
}

case_empty_statements_are_ignored() {
  local sql
  sql=$'INSERT INTO users (name, age, major) VALUES (\'김민준\', 25, \'컴퓨터공학\');;;\nSELECT name FROM users;;\n'
  local expected
  expected=$'name\n김민준\n'
  run_sql_and_assert "$sql" "$expected"
}

case_whitespace_only_file_is_noop() {
  local sql
  sql=$'   \n\t  \n  \n'
  local expected
  expected=''
  run_sql_and_assert "$sql" "$expected"
}

case_semicolon_inside_quoted_value() {
  local sql
  sql=$'INSERT INTO users (name, age, major) VALUES (\'김;민준\', 25, \'컴퓨터공학\');\nSELECT name FROM users;\n'
  local expected
  expected=$'name\n김;민준\n'
  run_sql_and_assert "$sql" "$expected"
}

case_windows_crlf_input() {
  local sql
  sql=$'INSERT INTO users (name, age, major) VALUES (\'김민준\', 25, \'컴퓨터공학\');\r\nSELECT name,major FROM users;\r\n'
  local expected
  expected=$'name,major\n김민준,컴퓨터공학\n'
  run_sql_and_assert "$sql" "$expected"
}

case_multiline_statement() {
  local sql
  sql=$'INSERT INTO users (name, age, major)\nVALUES (\'김민준\', 25, \'컴퓨터공학\');\nSELECT name, age FROM users;\n'
  local expected
  expected=$'name,age\n김민준,25\n'
  run_sql_and_assert "$sql" "$expected"
}

run_case "input_blank_lines_are_ignored" case_blank_lines_are_ignored
run_case "input_whitespace_around_statements_is_ignored" case_whitespace_around_statements_is_ignored
run_case "input_multiple_statements_in_one_line" case_multiple_statements_in_one_line
run_case "input_last_statement_without_trailing_semicolon" case_last_statement_without_trailing_semicolon
run_case "input_empty_statements_are_ignored" case_empty_statements_are_ignored
run_case "input_whitespace_only_file_is_noop" case_whitespace_only_file_is_noop
run_case "input_semicolon_inside_quoted_value" case_semicolon_inside_quoted_value
run_case "input_windows_crlf_input" case_windows_crlf_input
run_case "input_multiline_statement" case_multiline_statement

echo ""
echo "Total: $TOTAL, Pass: $PASS, Fail: $FAIL"

[ "$FAIL" -eq 0 ]
