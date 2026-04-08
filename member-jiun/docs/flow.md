# SQL Processor Flow

## 1. 프로그램 시작
1. `./sql_processor <sql_file>` 인자를 검증합니다.
2. 인자 오류(없음/과다) 또는 파일 열기 실패 시 `ERROR: file open failed`를 `stderr`에 출력하고 종료코드 `1`로 종료합니다.

## 2. 초기화
1. 필수 스키마(`users`, `products`)를 로드합니다.
2. 스토리지를 초기화합니다. 현재 구현은 CSV 기반 저장소를 사용합니다.
3. 초기화 실패 시 에러를 출력하고 종료코드 `1`로 종료합니다.

## 3. 입력 처리
1. SQL 파일 내용을 읽습니다.
2. 빈 줄/공백을 정리합니다.
3. `;` 기준으로 SQL 문장을 분리합니다.

## 4. 문장별 실행 루프
1. 각 SQL 문을 파싱합니다. 지원 범위는 `INSERT`, `SELECT`입니다.
2. 파싱 실패 시 `ERROR: invalid query`를 출력하고 다음 문장으로 진행합니다.

## 5. INSERT 처리
1. 테이블 존재 여부를 검증합니다. 없으면 `ERROR: table not found`를 출력합니다.
2. 컬럼/값 개수를 검증합니다. 불일치하면 `ERROR: column count does not match value count`를 출력합니다.
3. 검증 통과 시 레코드를 파일에 append합니다.
4. 성공 시 출력은 없습니다.

## 6. SELECT 처리
1. 테이블/컬럼 유효성을 검증합니다. 잘못된 컬럼은 `ERROR: invalid query`를 출력합니다.
2. 데이터 파일을 조회합니다.
3. 결과가 있으면 `헤더 + 데이터 행`을 CSV로 출력합니다.
4. 결과가 없으면 출력하지 않습니다.

## 7. 에러 및 종료 처리
1. 한 SQL 문장에서는 대표 에러 1개만 출력합니다.
2. 에러가 발생해도 다음 SQL 문장을 계속 실행합니다.
3. 전체 실행 중 에러가 1건 이상이면 종료코드 `1`, 없으면 `0`으로 종료합니다.

## 8. 확장 포인트
1. 스토리지 인터페이스를 유지하면 백엔드를 교체할 수 있습니다.
2. CSV 구현을 SQLite 구현으로 교체해 동시성/확장 요구에 대응할 수 있습니다.

## 9. 단계별 테스트 매핑
1. 프로그램 시작 단계는 [run_cli_tests.sh](/Users/nako/jungle/wed_coding_session/jungle-week6-coding-session/member-jiun/tests/cli/run_cli_tests.sh)에서 검증합니다.
- `cli_missing_file_returns_error`
- `cli_directory_path_returns_error`
- `cli_permission_denied_returns_error`
- `cli_empty_sql_file_success`
- `cli_no_args_should_fail`
- `cli_too_many_args_should_fail`
2. 초기화 단계는 [run_init_tests.sh](/Users/nako/jungle/wed_coding_session/jungle-week6-coding-session/member-jiun/tests/init/run_init_tests.sh)에서 검증합니다.
- `init_with_required_schema_success`
- `init_missing_users_schema_should_fail`
- `init_missing_products_schema_should_fail`
- `init_malformed_schema_should_fail`
- `init_unreadable_schema_should_fail`
3. 입력 처리 단계는 [run_input_tests.sh](/Users/nako/jungle/wed_coding_session/jungle-week6-coding-session/member-jiun/tests/input/run_input_tests.sh)에서 검증합니다.
- `input_blank_lines_are_ignored`
- `input_whitespace_around_statements_is_ignored`
- `input_multiple_statements_in_one_line`
4. 문장별 실행 루프 단계는 [run_parse_tests.sh](/Users/nako/jungle/wed_coding_session/jungle-week6-coding-session/member-jiun/tests/parse/run_parse_tests.sh)에서 검증합니다.
- `parse_supported_keywords_case_insensitive`
- `parse_invalid_statement_reports_error`
- `parse_invalid_query_continues_to_next_statement`
- `parse_multiple_invalid_statements_report_per_statement`
5. INSERT/SELECT 상세 처리(테이블/컬럼 검증, 저장/조회 포맷)는 다음 테스트 단계에서 추가 예정입니다.
5. INSERT 처리 단계는 [run_insert_tests.sh](/Users/nako/jungle/wed_coding_session/jungle-week6-coding-session/member-jiun/tests/insert/run_insert_tests.sh)에서 검증합니다.
- `insert_success_is_silent_and_appended`
- `insert_table_not_found`
- `insert_column_value_mismatch`
- `insert_preserves_spaces_and_comma_in_quoted_values`
- `insert_error_does_not_stop_next_statements`
6. SELECT 상세 처리와 종료코드 집계 세부 시나리오는 다음 테스트 단계에서 추가 예정입니다.
