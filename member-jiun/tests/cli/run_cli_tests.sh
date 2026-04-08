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

case_missing_file() {
  tmp="$(mktemp -d)"
  trap 'rm -rf "$tmp"' RETURN

  local out_file="$tmp/stdout.txt"
  local err_file="$tmp/stderr.txt"

  set +e
  "$BIN" "$tmp/not_exists.sql" >"$out_file" 2>"$err_file"
  local code=$?
  set -e

  local expected_err="ERROR: file open failed"
  local actual_err
  actual_err="$(tr -d '\r' < "$err_file" | sed '/^$/d')"

  [ "$code" -eq 1 ] || return 1
  [ ! -s "$out_file" ] || return 1
  [ "$actual_err" = "$expected_err" ] || return 1

  return 0
}

case_directory_path() {
  tmp="$(mktemp -d)"
  trap 'rm -rf "$tmp"' RETURN

  local out_file="$tmp/stdout.txt"
  local err_file="$tmp/stderr.txt"

  set +e
  "$BIN" "$tmp" >"$out_file" 2>"$err_file"
  local code=$?
  set -e

  local expected_err="ERROR: file open failed"
  local actual_err
  actual_err="$(tr -d '\r' < "$err_file" | sed '/^$/d')"

  [ "$code" -eq 1 ] || return 1
  [ ! -s "$out_file" ] || return 1
  [ "$actual_err" = "$expected_err" ] || return 1

  return 0
}

case_permission_denied_file() {
  tmp="$(mktemp -d)"
  trap 'chmod 644 "$tmp/input.sql" 2>/dev/null || true; rm -rf "$tmp"' RETURN

  cat >"$tmp/input.sql" <<'EOF'
SELECT * FROM users;
EOF
  chmod 000 "$tmp/input.sql"

  local out_file="$tmp/stdout.txt"
  local err_file="$tmp/stderr.txt"

  set +e
  "$BIN" "$tmp/input.sql" >"$out_file" 2>"$err_file"
  local code=$?
  set -e

  local expected_err="ERROR: file open failed"
  local actual_err
  actual_err="$(tr -d '\r' < "$err_file" | sed '/^$/d')"

  [ "$code" -eq 1 ] || return 1
  [ ! -s "$out_file" ] || return 1
  [ "$actual_err" = "$expected_err" ] || return 1

  return 0
}

case_empty_sql_file() {
  tmp="$(mktemp -d)"
  trap 'rm -rf "$tmp"' RETURN

  : >"$tmp/empty.sql"

  local out_file="$tmp/stdout.txt"
  local err_file="$tmp/stderr.txt"

  set +e
  "$BIN" "$tmp/empty.sql" >"$out_file" 2>"$err_file"
  local code=$?
  set -e

  [ "$code" -eq 0 ] || return 1
  [ ! -s "$out_file" ] || return 1
  [ ! -s "$err_file" ] || return 1

  return 0
}

case_no_args_should_fail() {
  tmp="$(mktemp -d)"
  trap 'rm -rf "$tmp"' RETURN

  local out_file="$tmp/stdout.txt"
  local err_file="$tmp/stderr.txt"

  set +e
  "$BIN" >"$out_file" 2>"$err_file"
  local code=$?
  set -e

  local expected_err="ERROR: file open failed"
  local actual_err
  actual_err="$(tr -d '\r' < "$err_file" | sed '/^$/d')"

  [ "$code" -eq 1 ] || return 1
  [ ! -s "$out_file" ] || return 1
  [ "$actual_err" = "$expected_err" ] || return 1

  return 0
}

case_too_many_args_should_fail() {
  tmp="$(mktemp -d)"
  trap 'rm -rf "$tmp"' RETURN

  cat >"$tmp/one.sql" <<'EOF'
SELECT * FROM users;
EOF
  cat >"$tmp/two.sql" <<'EOF'
SELECT * FROM products;
EOF

  local out_file="$tmp/stdout.txt"
  local err_file="$tmp/stderr.txt"

  set +e
  "$BIN" "$tmp/one.sql" "$tmp/two.sql" >"$out_file" 2>"$err_file"
  local code=$?
  set -e

  local expected_err="ERROR: file open failed"
  local actual_err
  actual_err="$(tr -d '\r' < "$err_file" | sed '/^$/d')"

  [ "$code" -eq 1 ] || return 1
  [ ! -s "$out_file" ] || return 1
  [ "$actual_err" = "$expected_err" ] || return 1

  return 0
}

run_case "cli_missing_file_returns_error" case_missing_file
run_case "cli_directory_path_returns_error" case_directory_path
run_case "cli_permission_denied_returns_error" case_permission_denied_file
run_case "cli_empty_sql_file_success" case_empty_sql_file
run_case "cli_no_args_should_fail" case_no_args_should_fail
run_case "cli_too_many_args_should_fail" case_too_many_args_should_fail

echo ""
echo "Total: $TOTAL, Pass: $PASS, Fail: $FAIL"

[ "$FAIL" -eq 0 ]
