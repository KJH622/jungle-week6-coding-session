# member-jiun 테스트 자산

이 디렉토리는 `member-jiun` 구현을 위한 독립 테스트 공간입니다.

## 구조
- `cli/`: CLI/프로세스 동작 검증
- `init/`: 초기화(스키마 로드/스토리지 준비) 검증
- `input/`: SQL 입력 정리/문장 분리 검증
- `fixtures/`: 테스트 입력 파일들

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
