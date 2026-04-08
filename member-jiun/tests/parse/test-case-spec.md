# 파싱/실행 루프 테스트 케이스 명세

이 문서는 `run_parse_tests.sh`에서 실행하는 문장별 실행 루프(파싱/에러 지속 실행) 테스트를 설명합니다.

## 1) `parse_supported_keywords_case_insensitive`
- 시나리오:
  - `insert`, `Select`처럼 대소문자가 섞인 키워드로 SQL을 실행합니다.
- 의도:
  - `INSERT`, `SELECT` 지원 키워드가 대소문자 비구분으로 파싱되는지 확인합니다.
- 검사:
  - 종료코드 `0`
  - `stderr` 비어 있음
  - `stdout`이 기대 결과와 정확히 일치

## 2) `parse_invalid_statement_reports_error`
- 시나리오:
  - 지원하지 않는 문장(`UPDATE`)을 실행합니다.
- 의도:
  - 비지원/파싱 실패 문장이 `ERROR: invalid query`로 처리되는지 확인합니다.
- 검사:
  - 종료코드 `1`
  - `stdout` 비어 있음
  - `stderr`가 `ERROR: invalid query` 1줄과 정확히 일치

## 3) `parse_invalid_query_continues_to_next_statement`
- 시나리오:
  - 첫 문장은 잘못된 쿼리, 다음 문장은 정상 `INSERT`/`SELECT`로 실행합니다.
- 의도:
  - 에러 발생 후에도 다음 문장을 계속 실행하는지 확인합니다.
- 검사:
  - 종료코드 `1` (에러 발생 이력 반영)
  - `stderr`는 `ERROR: invalid query` 1줄
  - `stdout`은 후속 정상 문장 결과와 일치

## 4) `parse_multiple_invalid_statements_report_per_statement`
- 시나리오:
  - 연속된 잘못된 문장 2개를 실행합니다.
- 의도:
  - 문장 단위로 대표 에러가 각각 1개씩 출력되는지 확인합니다.
- 검사:
  - 종료코드 `1`
  - `stdout` 비어 있음
  - `stderr`가 `ERROR: invalid query` 2줄과 정확히 일치
