# Feature Spec: Mini SQL Processor Phase 2

## Goal

- `.sql` 파일의 `INSERT`, `SELECT`, `DELETE`, `UPDATE`를 실행한다.
- 현재 디렉토리의 `.schema` 파일을 읽어 테이블을 인식한다.
- `INSERT`는 `<table>.data` 파일에 row를 저장한다.
- `SELECT`는 `<table>.data` 파일에서 row를 읽고 `WHERE`, `ORDER BY`, `LIMIT`를 적용해 출력한다.
- `DELETE`, `UPDATE`는 파일 전체를 다시 써서 결과를 반영한다.
- exact string 오류 규약으로 기존 public/hidden 테스트를 계속 통과한다.

## Success Criteria

- [x] 기존 public 6개 / hidden 8개 테스트를 모두 유지한다.
- [x] `INSERT` 컬럼 순서가 달라도 schema 순서로 재배열해 저장한다.
- [x] `SELECT`는 단일 `WHERE`, 단일 `ORDER BY`, `LIMIT`를 지원한다.
- [x] `DELETE FROM table;` 와 `DELETE FROM table WHERE ...;` 를 지원한다.
- [x] `UPDATE table SET ...;` 와 `UPDATE table SET ... WHERE ...;` 를 지원한다.
- [x] 새 기능도 기존 exact error strings 4개 안에서 처리한다.

## Constraints

- 구현은 `member-dragon/src/`에만 둔다.
- `common/`은 수정하지 않는다.
- DB, 네트워크, 외부 저장소는 구현하지 않는다.
- `AND`, `OR`, `JOIN`, `GROUP BY`, 집계 함수, 서브쿼리는 지원하지 않는다.

## Validation

- `cd member-dragon/src && make clean && make`
- `./common/scripts/run_tests.sh ./member-dragon/src/sql_processor public`
- `./common/scripts/run_tests.sh ./member-dragon/src/sql_processor hidden`
- 수동 시나리오로 `WHERE`, `ORDER BY`, `LIMIT`, `UPDATE`, `DELETE`, reordered `INSERT`를 확인한다.

## Risks

- 고정 최대 크기(`MAX_ROWS`, `MAX_COLUMNS`)를 넘는 입력은 현재 범위 밖이다.
- `WHERE`는 단일 조건만 지원한다.
- `DELETE`/`UPDATE`는 파일 전체 rewrite 방식이라 대용량 데이터에는 비효율적일 수 있다.
- 단일 따옴표 이스케이프 같은 확장 SQL 문법은 아직 지원하지 않는다.
