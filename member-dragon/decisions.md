# 설계 판단 기록

## 저장 포맷
- **결정**: 각 테이블을 `<table>.data` 파일로 저장하는 파일 기반 저장소를 사용한다.
- **이유**: 원 과제 요구사항이 `INSERT`는 파일에 저장, `SELECT`는 파일에서 읽기를 명시하고 있어 저장 계층을 메모리가 아니라 파일로 구현해야 했기 때문이다.
- **트레이드오프**: 경량 custom 포맷(length-prefixed row encoding)을 직접 설계해야 했고, 포맷 해석 실패 시 파일 I/O 오류 처리도 같이 고려해야 했다.

## 파서 구조
- **결정**: `main.c`에서 세미콜론 기준으로 statement를 분리하고, `parser.c`는 단일 statement만 해석하는 구조로 나눴다.
- **이유**: 한 줄씩만 읽는 구현의 한계를 피하면서도 파서를 단순하게 유지할 수 있다.
- **트레이드오프**: SQL 전체 문법을 지원하는 범용 파서는 아니고, 현재 프로젝트 문법에 맞춘 전용 파서다.

## 에러 처리 범위
- **결정**: `invalid query`, `table not found`, `column count does not match value count`, `file open failed` 네 가지 exact string만 출력한다.
- **이유**: 공통 문서와 테스트가 문자열까지 정확히 비교하므로 출력 표준을 흔들 수 없다.
- **포기한 것**: 타입 검증, 세부 원인 메시지, 다중 에러 보고는 이번 범위에서 제외했다.

## WHERE절 지원 여부
- **초기 결정**: 지원하지 않는다.
- **초기 이유**: 공통 테스트 범위가 아니고, 1차 목표는 핵심 `INSERT`/`SELECT` 안정화였다.
- **변경 결정**: Phase 2에서 단일 조건 `WHERE`를 지원한다.
- **변경 이유**: 추가 구현 범위를 넓히되, `AND`/`OR` 없이도 `SELECT`, `DELETE`, `UPDATE`의 핵심 확장을 보여 줄 수 있기 때문이다.

## 기타
- **vtable 저장소 분리**: `StorageOps`와 `file_storage.c`를 분리해 현재는 파일 저장소를 쓰되 이후 다른 백엔드로 교체할 여지를 남겼다.
- **ResultSet 메모리 정책**: `ResultSet`은 고정 크기 구조체로 두되 `SELECT` 실행 시 힙에 올려 스택 오버플로를 피했다.
- **INSERT 컬럼 검증**: 컬럼 목록을 완전히 무시하지 않고, 개수 불일치는 column mismatch, 같은 개수에서 이름/순서 불일치는 invalid query로 분리했다.
- **파일 저장 포맷**: 값 안의 콤마를 안전하게 처리하기 위해 CSV 대신 각 필드를 `길이:값` 형태로 이어 붙인 custom row 포맷을 사용했다.

## Phase 2 실행 파이프라인
- **결정**: `WHERE`와 `ORDER BY`는 projection 이후가 아니라 "전체 row 기준"으로 먼저 처리한다.
- **이유**: `SELECT name FROM users WHERE age > 22`처럼 조건 컬럼이 출력 컬럼에 없을 수 있기 때문이다.
- **트레이드오프**: executor가 full-row `ResultSet`을 다루는 코드가 늘었지만, 규칙은 한 곳으로 모였다.

## StorageOps 확장 방식
- **결정**: `delete_rows`, `update_rows` 같은 조건 전용 함수를 추가하지 않고, `read_all_rows`와 `replace_rows`만 추가했다.
- **이유**: 저장소는 파일 I/O만 맡고, 조건 해석/정렬/제한은 executor에서 일관되게 처리하는 편이 더 단순하다.
- **트레이드오프**: `DELETE`/`UPDATE`는 파일 전체를 다시 써야 하므로 대용량 데이터에는 비효율적일 수 있다.

## INSERT 컬럼 재정렬
- **결정**: `INSERT`는 schema 전체 컬럼을 모두 포함한다는 전제에서, 컬럼 순서가 달라도 schema 순서로 재배열해 저장한다.
- **이유**: `INSERT INTO users (major, name, age) ...` 같은 입력을 허용해 과제의 차별화 요소를 만들기 위함이다.
- **에러 규칙**: 중복 컬럼, 없는 컬럼, 누락 컬럼은 `invalid query`로 처리한다.

## 비교 방식
- **결정**: 두 값이 모두 정수처럼 읽히면 숫자 비교, 아니면 문자열 비교를 사용한다.
- **이유**: 별도 타입 시스템을 크게 늘리지 않고도 `WHERE age > 22`, `ORDER BY age DESC` 같은 기능을 자연스럽게 처리할 수 있다.

## 문자열 파싱 강화
- **결정**: statement 분리와 quoted string 파싱을 둘 다 quote-aware scanner 방식으로 보강하고, SQL escaped quote(`''`)를 지원한다.
- **이유**: `O''Reilly` 같은 값, 값 내부 세미콜론, 마지막 세미콜론 없는 문장, `WHERE age > 22 ORDER BY ...` 같은 구문 경계를 안전하게 처리하려면 문자열과 절 경계를 함께 이해해야 하기 때문이다.
- **트레이드오프**: parser가 조금 복잡해졌지만, bare value가 다음 절을 삼켜버리는 문제와 escaped quote 미지원을 동시에 해결할 수 있었다.
