# 인터랙티브 모드 테스트 케이스 명세

이 문서는 `run_interactive_tests.sh`에서 실행하는 인터랙티브 모드 테스트를 설명합니다.

## 1) `interactive_help_command`
- 시나리오:
  - 인터랙티브 모드에서 `.help` 후 `.quit`를 입력합니다.
- 검사:
  - 종료코드 `0`
  - `stderr` 비어 있음
  - `stdout`에 도움말과 특수 명령 목록이 포함됨

## 2) `interactive_tables_command`
- 시나리오:
  - 인터랙티브 모드에서 `.tables` 후 `.quit`를 입력합니다.
- 검사:
  - 종료코드 `0`
  - `stderr` 비어 있음
  - `stdout`에 현재 `.schema` 파일 기준 테이블 목록이 출력됨

## 3) `interactive_schema_command`
- 시나리오:
  - 인터랙티브 모드에서 `.schema users` 후 `.quit`를 입력합니다.
- 검사:
  - 종료코드 `0`
  - `stderr` 비어 있음
  - `stdout`에 `users` 스키마의 컬럼과 타입이 출력됨

## 4) `interactive_sql_execution`
- 시나리오:
  - 인터랙티브 모드에서 `INSERT`, `SELECT`, `.quit`를 순서대로 입력합니다.
- 검사:
  - 종료코드 `0`
  - `stderr` 비어 있음
  - `stdout`에 INSERT 성공 메시지, SELECT 헤더/데이터, row count가 포함됨
