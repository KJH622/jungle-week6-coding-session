# CLI 테스트 케이스 명세

이 문서는 `run_cli_tests.sh`에서 실행하는 CLI 테스트가 무엇을 검사하는지 정리합니다.

## 공통 검증 원칙
- 에러 케이스는 `stdout`이 비어 있어야 합니다.
- `stderr` 메시지는 정책에 맞아야 합니다.
- 종료 코드는 요구사항과 일치해야 합니다.

## 테스트 케이스

### 1) `cli_missing_file_returns_error`
- 시나리오: 존재하지 않는 SQL 파일 경로로 실행
- 검사:
  - 종료코드 `1`
  - `stdout` 비어 있음
  - `stderr`가 정확히 `ERROR: file open failed`

### 2) `cli_directory_path_returns_error`
- 시나리오: 파일 대신 디렉토리 경로를 전달
- 검사:
  - 종료코드 `1`
  - `stdout` 비어 있음
  - `stderr`가 정확히 `ERROR: file open failed`

### 3) `cli_permission_denied_returns_error`
- 시나리오: 읽기 권한이 없는 SQL 파일을 전달
- 검사:
  - 종료코드 `1`
  - `stdout` 비어 있음
  - `stderr`가 정확히 `ERROR: file open failed`

### 4) `cli_empty_sql_file_success`
- 시나리오: 빈 SQL 파일 실행
- 검사:
  - 종료코드 `0`
  - `stdout` 비어 있음
  - `stderr` 비어 있음

### 5) `cli_no_args_should_fail`
- 시나리오: 인자 없이 실행
- 검사:
  - 종료코드 `1`
  - `stdout` 비어 있음
  - `stderr`가 정확히 `ERROR: file open failed`

### 6) `cli_too_many_args_should_fail`
- 시나리오: SQL 파일 인자를 2개 이상 전달
- 검사:
  - 종료코드 `1`
  - `stdout` 비어 있음
  - `stderr`가 정확히 `ERROR: file open failed`
