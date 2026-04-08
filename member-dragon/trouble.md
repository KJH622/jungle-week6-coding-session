# 삽질 로그

## 이슈 1: 첫 공개 테스트에서 SELECT가 세그폴트 남
- **상황**: 빌드는 성공했는데 `01_basic` 공개 테스트에서 첫 `SELECT` 실행 시 segmentation fault가 발생했다.
- **원인**: `executor.c`에서 `ResultSet`을 스택에 직접 잡았는데, 고정 크기 버퍼가 커서 macOS 스택 한계를 넘겼다.
- **AI가 준 답**: `ResultSet` 메모리 소유권을 명확히 하고, 큰 결과 버퍼는 스택 대신 힙으로 옮기자고 제안했다.
- **실제 해결**: `execute_command()`에서 `ResultSet`을 `malloc`으로 생성하고 사용 후 `free`하도록 바꿨다.
- **배운 것**: 고정 크기 구조체는 단순하지만, 크기가 커지면 스택/힙 배치를 먼저 따져야 한다.

## 이슈 2: hidden `08_mixed_errors`에서 에러 문자열이 하나 틀림
- **상황**: 숨은 테스트는 거의 다 통과했지만, `INSERT INTO users (name, age) ...` 케이스에서 `ERROR: invalid query`가 나와 실패했다.
- **원인**: INSERT 컬럼 검증을 너무 먼저 강하게 적용하면서, 이 프로젝트가 기대하는 `column count does not match value count` 우선순위를 덮어버렸다.
- **AI가 준 답**: 컬럼 목록을 계속 검증하되, 개수 불일치는 column mismatch로, 같은 개수의 이름/순서 오류만 invalid query로 분리하자고 정리했다.
- **실제 해결**: `validate_insert_shape()`의 반환 우선순위를 수정해 count mismatch는 `ERR_COLUMN_MISMATCH`로 매핑했다.
- **배운 것**: SQL 엔진의 정확도만큼 중요한 게 테스트가 기대하는 에러 우선순위다.

## 이슈 3: 과제 요구사항은 파일 저장인데 초기 플랜이 메모리 저장으로 기울어 있었음
- **상황**: 공통 테스트는 통과했지만, 원 과제 설명을 다시 확인해 보니 `INSERT`는 파일 저장, `SELECT`는 파일 읽기를 요구하고 있었다.
- **원인**: 테스트 통과 중심으로 생각하면서 저장 계층을 메모리로 단순화한 초안이 그대로 구현 방향에 남아 있었다.
- **AI가 준 답**: `StorageOps` vtable은 유지한 채 backend를 `file_storage.c`로 교체하고, 각 테이블을 `<table>.data` 파일로 관리하도록 수정하자고 제안했다.
- **실제 해결**: 메모리 저장소를 제거하고 file-backed 저장소를 구현해 `INSERT`는 `.data` 파일 append, `SELECT`는 `.data` 파일 read로 바꿨다.
- **배운 것**: 테스트를 통과해도 원 요구사항과 어긋날 수 있으니, 마지막엔 반드시 과제 본문과 구현을 다시 대조해야 한다.

## 이슈 4: Phase 2 계획대로라면 WHERE/ORDER BY가 projection 이후에 적용되어 잘못 동작할 뻔함
- **상황**: 새 계획 문서를 검토하다 보니 `SELECT name FROM users WHERE age > 22` 같은 쿼리를 executor가 projection 이후 `ResultSet`에서 걸러내는 흐름이었다.
- **원인**: 기존 `select_rows()` 인터페이스를 최대한 그대로 유지하려다 보니, "출력용 컬럼"과 "조건/정렬용 컬럼"을 같은 단계에서 다루려는 설계가 섞였다.
- **AI가 준 답**: 저장소에 `read_all_rows`와 `replace_rows`만 추가하고, executor가 전체 row를 읽은 뒤 `WHERE -> ORDER BY -> LIMIT -> projection` 순서로 처리하자고 정리했다.
- **실제 해결**: `StorageOps`를 additively 확장하고, `executor.c`에서 full-row 파이프라인으로 `SELECT`, `DELETE`, `UPDATE`를 다시 구성했다.
- **배운 것**: SQL의 출력 컬럼과 조건 컬럼은 다를 수 있으므로, projection은 되도록 마지막 단계에 두는 편이 안전하다.

## 이슈 5: INSERT 컬럼 재정렬만 넣으면 중복 컬럼이 조용히 통과할 수 있었음
- **상황**: `INSERT INTO users (name, name, major) ...` 같은 입력이 들어오면 같은 schema 칸이 두 번 채워지고 다른 칸은 비는 문제가 생길 수 있었다.
- **원인**: 초기 확장안은 "이름에 맞춰 자리를 찾아 넣는다"까지는 있었지만, 같은 컬럼이 두 번 들어오는 경우를 별도로 막지 않았다.
- **AI가 준 답**: schema 인덱스 기준 `seen[]` 배열을 두고, 이미 채운 컬럼이 다시 나오면 바로 `invalid query`로 처리하자고 제안했다.
- **실제 해결**: `executor.c`의 reorder 단계에서 중복 컬럼과 누락 컬럼을 모두 검사하도록 바꿨다.
- **배운 것**: "재정렬" 기능은 편의성만 늘리는 게 아니라, 잘못된 입력을 더 꼼꼼히 검증해야 한다.

## 이슈 6: 문자열 파싱이 `O''Reilly`와 절 경계를 제대로 처리하지 못했음
- **상황**: `O''Reilly` 같은 SQL escaped quote는 `invalid query`가 났고, `WHERE age > 22 ORDER BY age DESC LIMIT 2`처럼 따옴표 없는 값 뒤 절이 붙는 경우엔 뒷부분이 값으로 빨려 들어갈 수 있었다.
- **원인**: `main.c`의 statement splitter는 작은따옴표를 볼 때마다 단순히 in/out 상태만 뒤집었고, `parser.c`의 `parse_value_token()`은 quoted string의 `''`를 이해하지 못했다. 또 unquoted value는 공백에서 멈추지 않아 다음 절을 삼킬 수 있었다.
- **AI가 준 답**: statement 분리와 quoted string 읽기를 둘 다 문자 단위 scanner로 바꾸고, `''`는 값 안의 작은따옴표 1개로 처리하자고 제안했다. bare value는 공백/구분자에서 멈추도록 하자는 방향도 함께 제시했다.
- **실제 해결**: `main.c`에서 escaped quote를 고려한 quote-aware statement splitter를 넣고, `parser.c`에 `parse_quoted_value()`를 추가해 `''`를 지원했다. unquoted value는 공백, `,`, `)`, `;`에서 멈추도록 바꿨다.
- **배운 것**: SQL 파싱은 "따옴표 안 문자열"과 "문장/절 경계"를 같이 봐야 안전하다. 한쪽만 고치면 다른 쪽에서 다시 깨질 수 있다.
