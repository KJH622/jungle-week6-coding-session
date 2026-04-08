# Domain Rules

이 프로젝트는 파일 기반 SQL 처리기다. 규칙은 `common/` 문서와 테스트를 우선한다.

## Scope

- 지원 SQL: `INSERT`, `SELECT`, `DELETE`, `UPDATE`
- 지원 절: 단일 `WHERE`, 단일 `ORDER BY`, `LIMIT`
- 범위 밖: `JOIN`, `GROUP BY`, 집계 함수, `AND`, `OR`, 서브쿼리
- 구현은 파일 기반 저장소를 사용한다.

## Schema Rules

- 스키마는 현재 작업 디렉토리의 `*.schema` 파일에서 읽는다.
- 데이터는 현재 작업 디렉토리의 `<table>.data` 파일에서 읽고 쓴다.
- 테이블 이름을 찾지 못하면 `ERROR: table not found`를 출력한다.

## Parse Rules

- SQL 키워드는 대소문자를 구분하지 않는다.
- `main.c`는 인용부호 바깥의 `;`를 기준으로 statement를 분리하고, `parser.c`는 단일 statement만 해석한다.
- `WHERE`는 조건 1개만 지원한다.
- `ORDER BY`는 컬럼 1개만 지원한다.
- `LIMIT`은 정수 1개만 지원한다.
- quoted string은 SQL 방식의 escaped quote(`''`)를 지원한다.
- 따옴표가 없는 값은 공백이나 다음 구분자에서 끝난 것으로 본다.
- quoted value 안의 공백과 콤마는 값의 일부로 취급한다.
- 빈 줄은 무시한다.

## Execution Rules

- `SELECT`는 항상 전체 row 기준으로 `WHERE -> ORDER BY -> LIMIT -> projection` 순서로 처리한다.
- `DELETE`와 `UPDATE`는 전체 row를 읽어 바꾼 뒤 파일 전체를 다시 쓴다.
- `INSERT`는 schema 전체 컬럼을 모두 포함해야 하며, 컬럼 순서는 달라도 schema 순서로 재배열해 저장한다.

## Error Priority

- 문법을 해석할 수 없으면 `ERROR: invalid query`를 출력한다.
- 테이블이 없으면 `ERROR: table not found`를 출력한다.
- `INSERT` 컬럼 수나 값 수가 schema 전체 row와 맞지 않으면 `ERROR: column count does not match value count`를 출력한다.
- 없는 컬럼을 `SELECT`, `WHERE`, `ORDER BY`, `SET`에서 사용하면 `ERROR: invalid query`를 출력한다.
- `INSERT`의 중복 컬럼, 없는 컬럼, 누락 컬럼은 `ERROR: invalid query`를 출력한다.

## Output Rules

- `INSERT`, `UPDATE`, `DELETE` 성공은 silent다.
- `SELECT`가 빈 결과면 아무것도 출력하지 않는다.
- `SELECT` 결과는 헤더 다음 행들을 모두 stdout에 출력한다.
