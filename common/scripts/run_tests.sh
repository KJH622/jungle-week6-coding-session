#!/bin/bash
# ============================================================
# SQL Processor 자동 채점 스크립트
# 
# 사용법:
#   ./run_tests.sh <sql_processor 경로> <테스트 디렉토리> [schema 디렉토리]
#
# 예시:
#   ./run_tests.sh ../member-a/sql_processor public
#   ./run_tests.sh ../member-b/sql_processor hidden
#   ./run_tests.sh ../member-a/sql_processor public ../schema
# ============================================================

set -euo pipefail

# --- 색상 ---
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

# --- 인자 확인 ---
if [ $# -lt 2 ]; then
    echo "사용법: $0 <sql_processor 경로> <public|hidden>"
    echo ""
    echo "예시:"
    echo "  $0 ../member-a/sql_processor public"
    echo "  $0 ../member-b/sql_processor hidden"
    exit 1
fi

PROCESSOR_INPUT="$1"
TEST_TYPE="$2"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SCHEMA_DIR="${3:-$SCRIPT_DIR/../schema}"
TEST_DIR="$SCRIPT_DIR/../tests/$TEST_TYPE"

# 실행 파일 경로를 절대경로로 정규화 (테스트 중 cd 영향 방지)
PROCESSOR="$(cd "$(dirname "$PROCESSOR_INPUT")" && pwd)/$(basename "$PROCESSOR_INPUT")"

# --- 실행파일 확인 ---
if [ ! -f "$PROCESSOR" ]; then
    echo -e "${RED}[ERROR] sql_processor를 찾을 수 없습니다: $PROCESSOR${NC}"
    exit 1
fi

if [ ! -x "$PROCESSOR" ]; then
    echo -e "${YELLOW}[WARN] 실행 권한 없음. chmod +x 시도...${NC}"
    chmod +x "$PROCESSOR"
fi

if [ ! -d "$TEST_DIR" ]; then
    echo -e "${RED}[ERROR] 테스트 디렉토리를 찾을 수 없습니다: $TEST_DIR${NC}"
    exit 1
fi

# timeout 호환성 처리 (Linux timeout, macOS gtimeout)
TIMEOUT_BIN=""
if command -v timeout >/dev/null 2>&1; then
    TIMEOUT_BIN="timeout"
elif command -v gtimeout >/dev/null 2>&1; then
    TIMEOUT_BIN="gtimeout"
else
    echo -e "${YELLOW}[WARN] timeout 명령이 없어 타임아웃 없이 실행합니다.${NC}"
fi

# --- 테스트 실행 ---
TOTAL=0
PASSED=0
FAILED=0
ERRORS=()

echo ""
echo -e "${CYAN}========================================${NC}"
echo -e "${CYAN}  SQL Processor 채점 ($TEST_TYPE)${NC}"
echo -e "${CYAN}========================================${NC}"
echo ""

# 임시 디렉토리 생성 (테스트마다 데이터 파일 격리)
WORK_DIR=$(mktemp -d)
trap "rm -rf $WORK_DIR" EXIT

for sql_file in "$TEST_DIR"/*.sql; do
    [ -f "$sql_file" ] || continue
    
    test_name=$(basename "$sql_file" .sql)
    expected_file="$TEST_DIR/${test_name}.expected"
    expected_err_file="$TEST_DIR/${test_name}.expected_err"
    
    TOTAL=$((TOTAL + 1))
    
    # 테스트별 격리된 작업 디렉토리
    test_work="$WORK_DIR/$test_name"
    mkdir -p "$test_work"
    
    # 스키마 파일 복사 (있으면)
    if [ -d "$SCHEMA_DIR" ]; then
        cp "$SCHEMA_DIR"/*.schema "$test_work/" 2>/dev/null || true
    fi
    
    # SQL 파일 복사
    cp "$sql_file" "$test_work/"
    
    # 실행 (타임아웃 5초)
    actual_stdout="$test_work/actual_stdout.txt"
    actual_stderr="$test_work/actual_stderr.txt"
    
    cd "$test_work"
    if [ -n "$TIMEOUT_BIN" ]; then
        "$TIMEOUT_BIN" 5 "$PROCESSOR" "$(basename "$sql_file")" \
            > "$actual_stdout" 2> "$actual_stderr" || true
    else
        "$PROCESSOR" "$(basename "$sql_file")" \
            > "$actual_stdout" 2> "$actual_stderr" || true
    fi
    cd - > /dev/null
    
    # --- stdout 비교 ---
    stdout_pass=true
    if [ -f "$expected_file" ]; then
        if ! diff -q "$expected_file" "$actual_stdout" > /dev/null 2>&1; then
            stdout_pass=false
        fi
    fi
    
    # --- stderr 비교 ---
    stderr_pass=true
    if [ -f "$expected_err_file" ]; then
        if ! diff -q "$expected_err_file" "$actual_stderr" > /dev/null 2>&1; then
            stderr_pass=false
        fi
    elif [ -s "$actual_stderr" ]; then
        # expected_err가 없는데 stderr가 있으면 실패 처리
        stderr_pass=false
    fi
    
    # --- 결과 판정 ---
    if $stdout_pass && $stderr_pass; then
        echo -e "  ${GREEN}✓ PASS${NC}  $test_name"
        PASSED=$((PASSED + 1))
    else
        echo -e "  ${RED}✗ FAIL${NC}  $test_name"
        FAILED=$((FAILED + 1))
        
        if ! $stdout_pass; then
            echo -e "    ${YELLOW}── stdout 차이:${NC}"
            diff --color=always "$expected_file" "$actual_stdout" 2>/dev/null | head -20 | sed 's/^/    /'
        fi
        
        if ! $stderr_pass; then
            echo -e "    ${YELLOW}── stderr 차이:${NC}"
            if [ -f "$expected_err_file" ]; then
                diff --color=always "$expected_err_file" "$actual_stderr" 2>/dev/null | head -20 | sed 's/^/    /'
            else
                echo -e "    ${YELLOW}   기대: (stderr 없음)${NC}"
                echo -e "    ${YELLOW}   실제:${NC}"
                sed -n '1,3p' "$actual_stderr" | sed 's/^/    /'
            fi
        fi
        echo ""
    fi
done

# --- 요약 ---
echo ""
echo -e "${CYAN}========================================${NC}"
echo -e "  총 ${TOTAL}개 | ${GREEN}통과 ${PASSED}개${NC} | ${RED}실패 ${FAILED}개${NC}"

if [ $TOTAL -gt 0 ]; then
    SCORE=$((PASSED * 100 / TOTAL))
    echo -e "  점수: ${SCORE}%"
fi

echo -e "${CYAN}========================================${NC}"
echo ""

# 전체 통과 시 종료코드 0, 아니면 1
[ $FAILED -eq 0 ] && exit 0 || exit 1
