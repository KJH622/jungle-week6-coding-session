# member-jiun 테스트 자산

이 디렉토리는 `member-jiun` 구현을 위한 독립 테스트 공간입니다.

## 구조
- `cli/`: CLI/프로세스 동작 검증
- `init/`: 초기화(스키마 로드/스토리지 준비) 검증
- `input/`: SQL 입력 정리/문장 분리 검증
- `parse/`: 문장별 파싱/에러 지속 실행 검증
- `insert/`: INSERT 실행/검증/에러 처리 검증
- `select/`: SELECT 실행/출력/에러 처리 검증
- `exit/`: 종료코드 집계/에러 출력 규칙 검증

## 현재 범위 (요구사항 v1 기준)
- 실행 형식: `./sql_processor <sql_file>`
- 입력 파일 오픈 실패 시 `ERROR: file open failed` + 종료코드 `1`
- 빈 SQL 파일은 정상 종료(출력 없음)
- 인자 없음/과다 인자는 `ERROR: file open failed` + 종료코드 `1`
- 인자 오류 시 추가 usage 출력은 하지 않음

## 실행
```bash
./member-jiun/tests/cli/run_cli_tests.sh ./member-jiun/src/sql_processor
./member-jiun/tests/init/run_init_tests.sh ./member-jiun/src/sql_processor
./member-jiun/tests/input/run_input_tests.sh ./member-jiun/src/sql_processor
./member-jiun/tests/parse/run_parse_tests.sh ./member-jiun/src/sql_processor
./member-jiun/tests/insert/run_insert_tests.sh ./member-jiun/src/sql_processor
./member-jiun/tests/select/run_select_tests.sh ./member-jiun/src/sql_processor
./member-jiun/tests/exit/run_exit_tests.sh ./member-jiun/src/sql_processor
```

## 포함 케이스 (CLI)
- 존재하지 않는 파일 경로
- 디렉토리 경로 전달
- 읽기 권한 없는 파일
- 빈 SQL 파일
- 인자 없음 (실패 동작 검증)
- 인자 2개 이상 (실패 동작 검증)

## 포함 케이스 (초기화)
- 필수 스키마(`users`, `products`) 존재 시 초기화 성공
- `users.schema` 누락 시 초기화 실패
- `products.schema` 누락 시 초기화 실패
- 스키마 JSON 손상 시 초기화 실패
- 스키마 읽기 권한 없음 시 초기화 실패

## 포함 케이스 (입력 처리)
- SQL 파일의 빈 줄 무시
- 문장 앞뒤 공백/탭 무시
- 한 줄 다중 문장(`;` 기준) 분리
- 마지막 문장 뒤 `;`가 없어도 처리
- 빈 문장(`;;`) 무시
- 공백만 있는 파일 no-op 처리
- 문자열 내부 `;` 처리
- Windows CRLF 개행 처리
- 멀티라인 SQL 문장 처리

## 포함 케이스 (파싱/실행 루프)
- `INSERT`/`SELECT` 키워드 대소문자 비구분 파싱
- 비지원 문장 `invalid query` 처리
- 파싱 에러 후 다음 문장 계속 실행
- 다중 파싱 에러의 문장 단위 출력

## 포함 케이스 (INSERT 처리)
- INSERT 성공 시 무출력 + append
- 존재하지 않는 테이블 INSERT 에러
- 컬럼/값 개수 불일치 에러
- 값 내부 공백/쉼표 보존
- INSERT 에러 후 다음 문장 계속 실행

## 포함 케이스 (SELECT 처리)
- `SELECT *` 헤더/행 출력
- 특정 컬럼 선택 및 컬럼 순서 유지
- 0건 조회 시 무출력
- 잘못된 컬럼 조회 시 `invalid query`
- 존재하지 않는 테이블 조회 시 `table not found`
- SELECT 에러 후 다음 문장 계속 실행

## 포함 케이스 (종료 처리)
- 전체 성공 시 종료코드 `0`
- 에러 1건 이상 시 종료코드 `1`
- 다중 에러 + 정상 문장 혼합 실행 시 계속 진행
- 문장당 대표 에러 1개 출력
