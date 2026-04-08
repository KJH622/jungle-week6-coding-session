# INSERT 처리 테스트 케이스 명세

이 문서는 `run_insert_tests.sh`에서 실행하는 INSERT 처리 테스트를 설명합니다.

## 1) `insert_success_is_silent_and_appended`
- 시나리오:
  - 같은 테이블에 `INSERT` 2회 실행 후 `SELECT *`를 수행합니다.
- 의도:
  - INSERT 성공 시 stdout 출력이 없고 데이터가 append 되는지 확인합니다.
- 검사:
  - 종료코드 `0`
  - `stderr` 비어 있음
  - `stdout`은 `SELECT` 결과만 포함하며 2개 레코드가 순서대로 출력됨

## 2) `insert_table_not_found`
- 시나리오:
  - 존재하지 않는 테이블(`orders`)에 INSERT를 수행합니다.
- 의도:
  - 테이블 존재 검증 실패 시 `table not found`를 출력하는지 확인합니다.
- 검사:
  - 종료코드 `1`
  - `stdout` 비어 있음
  - `stderr`가 `ERROR: table not found`와 정확히 일치

## 3) `insert_column_value_mismatch`
- 시나리오:
  - 컬럼 수와 값 수가 다른 INSERT를 수행합니다.
- 의도:
  - 컬럼/값 개수 불일치 에러를 정확히 출력하는지 확인합니다.
- 검사:
  - 종료코드 `1`
  - `stdout` 비어 있음
  - `stderr`가 `ERROR: column count does not match value count`와 정확히 일치

## 4) `insert_preserves_spaces_and_comma_in_quoted_values`
- 시나리오:
  - 따옴표 내부에 공백/쉼표가 포함된 문자열 값을 INSERT 후 SELECT로 조회합니다.
- 의도:
  - 값 내부 공백/쉼표를 손상 없이 저장/조회하는지 확인합니다.
- 검사:
  - 종료코드 `0`
  - `stderr` 비어 있음
  - `stdout`이 기대 결과와 정확히 일치

## 5) `insert_error_does_not_stop_next_statements`
- 시나리오:
  - 첫 INSERT는 실패(개수 불일치), 다음 INSERT는 성공, 마지막 SELECT를 수행합니다.
- 의도:
  - 에러 발생 후에도 다음 SQL 문장을 계속 실행하는지 확인합니다.
- 검사:
  - 종료코드 `1`
  - `stderr`는 실패한 첫 문장에 대한 에러 1줄
  - `stdout`은 후속 성공 INSERT 결과만 포함
