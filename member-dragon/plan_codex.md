# Mini SQL Parser - Phase 2 계획 및 보정 기록

## 목표

기존 파일 기반 SQL 처리기에 아래 기능을 추가한다.

- `DELETE`
- `UPDATE`
- `WHERE`
- 비교 연산자 `=`, `!=`, `>`, `<`, `>=`, `<=`
- `ORDER BY`
- `LIMIT`
- `INSERT` 컬럼 순서 재정렬

대상 구현 위치는 `member-dragon/src/`이며, `common/`은 수정하지 않는다.

## 원래 계획에서 문제였던 점과 수정 이유

| 문제 | 왜 위험했는가 | 실제 수정 |
|---|---|---|
| `SELECT`의 `WHERE`/`ORDER BY`를 projection 이후 `ResultSet`에서 처리하려고 했음 | `SELECT name FROM users WHERE age > 22`처럼 조건 컬럼이 출력 컬럼에 없으면 필터나 정렬이 깨진다 | `StorageOps`에 `read_all_rows`, `replace_rows`를 추가하고, executor가 전체 row를 먼저 읽어 처리한 뒤 마지막에 projection 하도록 바꿨다 |
| `DELETE`/`UPDATE`를 전용 vtable 함수로 바로 넣으려 했음 | 저장소가 조건 해석까지 떠안게 되고, `WHERE`/`ORDER BY`와 실행 규칙이 여러 곳으로 흩어진다 | 저장소는 "전체 row 읽기/전체 row 교체"만 책임지고, 조건 해석은 executor 한 곳에서 처리하도록 정리했다 |
| `INSERT` 컬럼 재정렬 계획에 중복 컬럼 검사 내용이 없었음 | `INSERT INTO users (name, name, major) ...` 같은 입력이 조용히 잘못 저장될 수 있다 | executor에서 schema 인덱스 기준 `seen[]` 검사를 넣어 중복/누락을 `invalid query`로 막는다 |
| 수동 검증을 `src/` 기준으로 적어 둠 | 이 프로그램은 현재 디렉토리의 `.schema`를 읽기 때문에 `src/`에서 바로 실행하면 실패하기 쉽다 | `playground` 또는 임시 디렉토리에 `.schema`를 둔 상태로 검증하도록 절차를 바꿨다 |

## 구현 전략

### 1. 타입 확장

`types.h`에 아래 구조를 추가한다.

- `CMD_DELETE`, `CMD_UPDATE`
- `WhereCondition`
- `OrderByClause`
- `SetClause`
- `StorageOps.read_all_rows`
- `StorageOps.replace_rows`

핵심 포인트는 기존 `select_rows`를 깨지 않고, 고급 기능에 필요한 "전체 row 기준 작업"만 추가하는 것이다.

### 2. 파서 확장

`parser.c`에서 아래를 지원한다.

- `SELECT ... [WHERE ...] [ORDER BY ...] [LIMIT ...]`
- `DELETE FROM ... [WHERE ...]`
- `UPDATE ... SET ... [WHERE ...]`

이번 범위의 문법 제한:

- `WHERE`는 조건 1개만 지원
- `AND`, `OR`, 괄호는 지원하지 않음
- `ORDER BY`는 컬럼 1개만 지원
- `LIMIT`은 정수 1개만 지원

### 3. 저장소 확장

`file_storage.c`는 두 단계로 동작한다.

1. `<table>.data` 전체를 읽어서 schema 순서의 `ResultSet`으로 만든다
2. 바뀐 `ResultSet`을 다시 `<table>.data` 파일 전체로 덮어쓴다

이 구조 덕분에 `DELETE`, `UPDATE`, `WHERE`, `ORDER BY`가 모두 같은 row 표현을 공유한다.

### 4. 실행기 확장

`executor.c`에서 기능별로 아래 순서로 처리한다.

#### INSERT

1. 테이블 존재 확인
2. 컬럼 수 / 값 수 / schema 컬럼 수 확인
3. 입력 컬럼을 schema 순서로 재배열
4. 중복 컬럼 / 없는 컬럼 검사
5. `ops->insert()` 호출

#### SELECT

1. `ops->read_all_rows()`로 전체 row 읽기
2. `WHERE` 컬럼 유효성 검사
3. `ORDER BY` 컬럼 유효성 검사
4. `WHERE` 필터 적용
5. `ORDER BY` 정렬 적용
6. `LIMIT` 적용
7. 마지막에 요청 컬럼만 projection
8. stdout 출력

#### DELETE

1. `ops->read_all_rows()`로 전체 row 읽기
2. `WHERE`가 있으면 matching row 제거
3. `WHERE`가 없으면 전체 삭제
4. `ops->replace_rows()`로 파일 다시 쓰기

#### UPDATE

1. `ops->read_all_rows()`로 전체 row 읽기
2. `SET` 대상 컬럼 검사
3. `WHERE`가 맞는 row만 값 수정
4. `ops->replace_rows()`로 파일 다시 쓰기

## 비교 규칙

- 두 값이 모두 정수처럼 해석되면 숫자 비교
- 아니면 문자열 비교

이 규칙은 `WHERE`와 `ORDER BY`에 공통으로 사용한다.

## 에러 규칙

기존 exact string 4개만 계속 사용한다.

- `ERROR: invalid query`
- `ERROR: table not found`
- `ERROR: column count does not match value count`
- `ERROR: file open failed`

새 기능에서의 매핑:

- 없는 테이블에 `DELETE`/`UPDATE`/`SELECT` -> `table not found`
- 없는 컬럼을 `WHERE`/`ORDER BY`/`SET`에서 사용 -> `invalid query`
- `INSERT`에서 컬럼 수/값 수/schema 수가 다름 -> `column count does not match value count`
- `INSERT`에서 컬럼 중복/없는 컬럼/누락 컬럼 -> `invalid query`

## 구현 순서

1. `types.h`에 새 타입과 vtable 추가
2. `parser.c`에 새 문법 추가
3. `file_storage.c`에 전체 row 읽기/교체 추가
4. `executor.c`에 full-row 기준 실행 파이프라인 추가
5. `main.c`에서 새 vtable 포인터 null 체크
6. 기존 public/hidden 테스트 재검증
7. 새 기능 수동 검증
8. `usage.md`, `decisions.md`, `trouble.md`, 하네스 문서 동기화

## 검증 절차

### 기존 테스트

```bash
cd member-dragon/src
make clean && make

cd /Users/kimyong/WorkSpace/mini-sql-parser/jungle-week6-coding-session
./common/scripts/run_tests.sh ./member-dragon/src/sql_processor public
./common/scripts/run_tests.sh ./member-dragon/src/sql_processor hidden
```

### 새 기능 수동 검증

`playground` 또는 임시 디렉토리에서 `.schema`를 둔 상태로 실행한다.

예시:

```bash
cd member-dragon/playground
./reset.sh
../src/sql_processor extended.sql
```

## 구현 완료 후 확인 결과

- `make clean && make` 성공
- public 6/6 PASS
- hidden 8/8 PASS
- 수동 검증으로 아래 시나리오 확인
  - `SELECT name FROM users WHERE age > 22 ORDER BY age DESC LIMIT 2`
  - `UPDATE users SET major = '전자공학' WHERE name = '김민준'`
  - `DELETE FROM users WHERE age < 22`
  - `INSERT INTO users (major, name, age) VALUES ('수학', '정현우', 24)`

## 추가 보강: 문자열 파싱 hardening

- `main.c`의 statement splitter는 escaped quote(`''`)를 문자열 종료로 오인하지 않도록 보강했다.
- `parser.c`는 quoted string에서 `''`를 실제 작은따옴표 1개로 복원한다.
- bare value는 공백, `,`, `)`, `;`에서 멈추도록 바꿔 `WHERE age > 22 ORDER BY ...` 같은 절 경계가 안전해졌다.
- 이 보강 후에도 public/hidden 회귀 테스트는 유지했고, 아래 케이스를 수동 검증했다.
  - 마지막 문장 세미콜론 없음
  - 값 내부 SQL 키워드
  - 빈 파일
  - 값 내부 세미콜론
  - `O''Reilly`
  - `WHERE ... ORDER BY ... LIMIT ...`

## 이번 범위에서 제외한 것

- `JOIN`
- `GROUP BY`
- 집계 함수
- `AND` / `OR`
- 다중 `ORDER BY`
- 서브쿼리
- escaped quote 같은 확장 SQL 문법
