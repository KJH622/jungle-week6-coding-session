# 입력 처리 테스트 케이스 명세

이 문서는 `run_input_tests.sh`에서 실행하는 입력 처리(Input Processing) 테스트를 설명합니다.

## 1) `input_blank_lines_are_ignored`
- 시나리오:
  - SQL 파일에 빈 줄이 여러 개 포함된 상태로 `INSERT`, `SELECT`를 실행합니다.
- 의도:
  - 빈 줄이 실행 의미를 바꾸지 않고 정상 처리되는지 확인합니다.
- 검사:
  - 종료코드 `0`
  - `stderr` 비어 있음
  - `stdout`이 기대 결과와 정확히 일치

## 2) `input_whitespace_around_statements_is_ignored`
- 시나리오:
  - 문장 앞뒤에 공백/탭이 포함된 SQL 파일을 실행합니다.
- 의도:
  - 문장 주변 공백이 있어도 SQL 문이 정상 인식되는지 확인합니다.
- 검사:
  - 종료코드 `0`
  - `stderr` 비어 있음
  - `stdout`이 기대 결과와 정확히 일치

## 3) `input_multiple_statements_in_one_line`
- 시나리오:
  - 한 줄에 여러 SQL 문장을 `;`로 이어 작성해 실행합니다.
- 의도:
  - `;` 기준 문장 분리가 정상 동작하는지 확인합니다.
- 검사:
  - 종료코드 `0`
  - `stderr` 비어 있음
  - `stdout`이 기대 결과와 정확히 일치

## 4) `input_last_statement_without_trailing_semicolon`
- 시나리오:
  - 파일 마지막 SQL 문장 뒤에 `;`가 없는 상태로 실행합니다.
- 의도:
  - 마지막 문장도 입력 처리 대상에 포함되는지 확인합니다.
- 검사:
  - 종료코드 `0`
  - `stderr` 비어 있음
  - `stdout`이 기대 결과와 정확히 일치

## 5) `input_empty_statements_are_ignored`
- 시나리오:
  - `;;`처럼 비어 있는 문장을 포함한 SQL 파일을 실행합니다.
- 의도:
  - 빈 문장이 실행 흐름을 깨지 않고 무시되는지 확인합니다.
- 검사:
  - 종료코드 `0`
  - `stderr` 비어 있음
  - `stdout`이 기대 결과와 정확히 일치

## 6) `input_whitespace_only_file_is_noop`
- 시나리오:
  - 공백/탭/개행만 있는 SQL 파일을 실행합니다.
- 의도:
  - 유효 문장이 없는 파일을 no-op으로 처리하는지 확인합니다.
- 검사:
  - 종료코드 `0`
  - `stdout` 비어 있음
  - `stderr` 비어 있음

## 7) `input_semicolon_inside_quoted_value`
- 시나리오:
  - 문자열 값 내부에 `;`가 포함된 SQL을 실행합니다.
- 의도:
  - 따옴표 내부 문자를 문장 구분자로 오인하지 않는지 확인합니다.
- 검사:
  - 종료코드 `0`
  - `stderr` 비어 있음
  - `stdout`이 기대 결과와 정확히 일치

## 8) `input_windows_crlf_input`
- 시나리오:
  - Windows 개행(`\r\n`)을 포함한 SQL 파일을 실행합니다.
- 의도:
  - 입력 정리 단계에서 CRLF 개행을 정상 처리하는지 확인합니다.
- 검사:
  - 종료코드 `0`
  - `stderr` 비어 있음
  - `stdout`이 기대 결과와 정확히 일치

## 9) `input_multiline_statement`
- 시나리오:
  - `INSERT`를 여러 줄로 나눠 작성한 SQL 파일을 실행합니다.
- 의도:
  - 멀티라인 문장을 하나의 SQL 문장으로 올바르게 처리하는지 확인합니다.
- 검사:
  - 종료코드 `0`
  - `stderr` 비어 있음
  - `stdout`이 기대 결과와 정확히 일치
