# SELECT 처리 테스트 케이스 명세

이 문서는 `run_select_tests.sh`에서 실행하는 SELECT 처리 테스트를 설명합니다.

## 1) `select_star_outputs_header_and_rows`
- 시나리오:
  - 데이터 삽입 후 `SELECT * FROM users;`를 실행합니다.
- 의도:
  - `SELECT *`가 헤더와 데이터 행을 올바르게 출력하는지 확인합니다.
- 검사:
  - 종료코드 `0`
  - `stderr` 비어 있음
  - `stdout`이 헤더+행 형식으로 기대 결과와 정확히 일치

## 2) `select_specific_columns_in_requested_order`
- 시나리오:
  - `SELECT major, name FROM users;`를 실행합니다.
- 의도:
  - 요청한 컬럼 순서대로 출력되는지 확인합니다.
- 검사:
  - 종료코드 `0`
  - `stderr` 비어 있음
  - `stdout`이 요청한 컬럼 순서를 유지

## 3) `select_empty_table_is_silent`
- 시나리오:
  - 데이터가 없는 테이블에서 `SELECT *`를 실행합니다.
- 의도:
  - 결과가 0건일 때 무출력(silent)인지 확인합니다.
- 검사:
  - 종료코드 `0`
  - `stdout` 비어 있음
  - `stderr` 비어 있음

## 4) `select_invalid_column_reports_invalid_query`
- 시나리오:
  - 존재하지 않는 컬럼을 포함한 SELECT를 실행합니다.
- 의도:
  - 컬럼 유효성 실패가 `ERROR: invalid query`로 처리되는지 확인합니다.
- 검사:
  - 종료코드 `1`
  - `stdout` 비어 있음
  - `stderr`가 `ERROR: invalid query`와 정확히 일치

## 5) `select_table_not_found`
- 시나리오:
  - 존재하지 않는 테이블을 대상으로 SELECT를 실행합니다.
- 의도:
  - 테이블 유효성 실패가 `ERROR: table not found`로 처리되는지 확인합니다.
- 검사:
  - 종료코드 `1`
  - `stdout` 비어 있음
  - `stderr`가 `ERROR: table not found`와 정확히 일치

## 6) `select_error_does_not_stop_next_statement`
- 시나리오:
  - 첫 문장에서 SELECT 에러를 유도하고, 이후 INSERT/SELECT를 연이어 실행합니다.
- 의도:
  - SELECT 에러 발생 후에도 다음 문장이 계속 실행되는지 확인합니다.
- 검사:
  - 종료코드 `1`
  - `stderr`는 첫 문장 에러 1줄
  - `stdout`은 후속 성공 문장 결과를 포함
