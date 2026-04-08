# Mini SQL Processor 사용법

## 위치

- 구현 코드: `member-dragon/src/`
- 실행 파일: `member-dragon/src/sql_processor`
- 수동 실행 폴더: `member-dragon/playground/`
- 공통 테스트: `common/tests/`
- 공통 스키마: `common/schema/`

## 빌드

```bash
cd member-dragon/src
make
```

빌드가 끝나면 `sql_processor` 실행 파일이 생성됩니다.

## 실행 전 주의

- 프로그램은 **현재 작업 디렉토리**의 `*.schema` 파일을 읽습니다.
- 그래서 직접 실행할 때는 실행 위치에 `users.schema`, `products.schema` 같은 스키마 파일이 있어야 합니다.
- 기본 실험용으로 `member-dragon/playground/`에 스키마와 샘플 SQL을 미리 넣어 두었습니다.
- 실행 중 `INSERT`, `UPDATE`, `DELETE`가 일어나면 현재 디렉토리의 `<table>.data` 파일이 생성되거나 갱신됩니다.

## 기본 실행

```bash
cd member-dragon/playground
../src/sql_processor sample.sql
```

데이터를 지우고 처음부터 다시 보고 싶으면:

```bash
cd member-dragon/playground
./reset.sh
../src/sql_processor sample.sql
```

## 지원 SQL

현재 지원 범위는 아래와 같습니다.

## 지원 키워드와 연산자

- SQL 키워드: `INSERT`, `INTO`, `VALUES`, `SELECT`, `FROM`, `WHERE`, `ORDER BY`, `ASC`, `DESC`, `LIMIT`, `DELETE`, `UPDATE`, `SET`
- 기타 문법 요소: `*`, `,`, `(`, `)`, `;`
- 비교 연산자: `=`, `!=`, `>`, `<`, `>=`, `<=`
- 키워드는 대소문자를 구분하지 않습니다.

한 줄로 보면 현재 프로그램이 이해하는 대표 문법은 아래와 같습니다.

```sql
INSERT INTO <table> (<col>, ...) VALUES (<value>, ...);
SELECT * FROM <table>;
SELECT <col>, ... FROM <table> WHERE <col> <op> <value> ORDER BY <col> ASC|DESC LIMIT <n>;
DELETE FROM <table> WHERE <col> <op> <value>;
UPDATE <table> SET <col> = <value>[, <col> = <value>] WHERE <col> <op> <value>;
```

### INSERT

```sql
INSERT INTO users (name, age, major) VALUES ('김민준', 25, '컴퓨터공학');
INSERT INTO users (major, name, age) VALUES ('수학', '정현우', 24);
INSERT INTO users (name, age, major) VALUES ('O''Reilly', 30, 'Books');
```

- schema의 모든 컬럼을 넣어야 합니다.
- 컬럼 순서는 달라도 됩니다.
- 중복 컬럼, 없는 컬럼, 누락 컬럼은 에러입니다.
- quoted string 안의 작은따옴표는 SQL 방식대로 `''`로 escape할 수 있습니다.

### SELECT

```sql
SELECT * FROM users;
SELECT name, major FROM users;
SELECT name FROM users WHERE age > 22;
SELECT * FROM users ORDER BY age DESC;
SELECT * FROM users ORDER BY age ASC LIMIT 2;
SELECT name, major FROM users WHERE age >= 22 ORDER BY name LIMIT 3;
```

### DELETE

```sql
DELETE FROM users WHERE age < 22;
DELETE FROM users;
```

- `WHERE`가 없으면 테이블의 모든 row를 삭제합니다.

### UPDATE

```sql
UPDATE users SET major = '전자공학' WHERE name = '김민준';
UPDATE users SET age = 26, major = '수학';
```

- `WHERE`가 없으면 모든 row를 수정합니다.
- `SET`에는 여러 컬럼을 넣을 수 있습니다.

## WHERE / ORDER BY / LIMIT 규칙

- `WHERE`는 조건 1개만 지원합니다.
- 지원 연산자: `=`, `!=`, `>`, `<`, `>=`, `<=`
- 따옴표가 없는 값은 공백이나 다음 절이 나오기 전까지만 읽습니다.
- `ORDER BY`는 컬럼 1개만 지원합니다.
- 정렬 방향은 `ASC`, `DESC`를 지원하고, 생략하면 기본값은 `ASC`입니다.
- `LIMIT`은 정수 1개만 지원합니다.
- `AND`, `OR`, 괄호, `JOIN`, `GROUP BY`, 집계 함수는 지원하지 않습니다.

## 출력 규칙

- `INSERT`, `UPDATE`, `DELETE` 성공 시 출력 없음
- `SELECT` 성공 시:
  - 첫 줄에 헤더 출력
  - 이후 데이터 행 출력
- 빈 결과 `SELECT`는 출력 없음
- 에러는 stderr로 아래 exact string 중 하나를 출력

```text
ERROR: invalid query
ERROR: table not found
ERROR: column count does not match value count
ERROR: file open failed
```

## 테스트

프로젝트 루트에서 실행합니다.

공개 테스트:

```bash
./common/scripts/run_tests.sh ./member-dragon/src/sql_processor public
```

히든 테스트:

```bash
./common/scripts/run_tests.sh ./member-dragon/src/sql_processor hidden
```

## playground에서 테스트 케이스 실행

단일 케이스:

```bash
cd member-dragon/playground
./run_case.sh public 01_basic
./run_case.sh hidden 06_comma_in_value
```

공개/히든 전체:

```bash
cd member-dragon/playground
./run_suite.sh public
./run_suite.sh hidden
```

## 새 기능 직접 확인 예시

`member-dragon/playground/extended.sql`에 확장 기능 예시를 미리 넣어 두었습니다.

```sql
INSERT INTO users (name, age, major) VALUES ('김민준', 25, '컴퓨터공학');
INSERT INTO users (name, age, major) VALUES ('이서연', 22, '경영학');
INSERT INTO users (name, age, major) VALUES ('박지호', 23, '물리학');
SELECT name FROM users WHERE age > 22 ORDER BY age DESC LIMIT 2;
UPDATE users SET major = '전자공학' WHERE name = '김민준';
SELECT * FROM users ORDER BY age ASC;
DELETE FROM users WHERE age < 23;
SELECT * FROM users;
```

실행:

```bash
cd member-dragon/playground
./reset.sh
../src/sql_processor extended.sql
```

## 새 키워드 종합 테스트 파일

이번에 추가한 키워드와 비교 연산자를 한 번씩 직접 돌려보려면 아래 파일을 쓰면 됩니다.

- 파일: `member-dragon/playground/phase2_keywords.sql`
- 기대 출력: `member-dragon/playground/phase2_keywords.expected.txt`
- 문자열 파싱 엣지케이스: `member-dragon/playground/parser_edge_cases.sql`
- 문자열 파싱 기대 출력: `member-dragon/playground/parser_edge_cases.expected.txt`
- 빈 파일 케이스: `member-dragon/playground/empty.sql`

실행:

```bash
cd member-dragon/playground
./reset.sh
../src/sql_processor phase2_keywords.sql
```

이 파일에는 아래가 모두 들어 있습니다.

- reordered `INSERT`
- `WHERE`의 `=`, `!=`, `>`, `<`, `>=`, `<=`
- `ORDER BY ASC`
- `ORDER BY DESC`
- `LIMIT`
- `UPDATE ... SET ... WHERE ...`
- `DELETE ... WHERE ...`
- `DELETE FROM table;`

기대 출력과 비교하고 싶으면:

```bash
cd member-dragon/playground
./reset.sh
../src/sql_processor phase2_keywords.sql > out.txt 2> err.txt
diff -u phase2_keywords.expected.txt out.txt
cat err.txt
```

문자열 파싱 전용 케이스를 보려면:

```bash
cd member-dragon/playground
./reset.sh
../src/sql_processor parser_edge_cases.sql > out.txt 2> err.txt
diff -u parser_edge_cases.expected.txt out.txt
cat err.txt

../src/sql_processor empty.sql > empty.out 2> empty.err
cat empty.out
cat empty.err
```

## 정리

- 다시 빌드: `cd member-dragon/src && make`
- 빌드 산출물 삭제: `cd member-dragon/src && make clean`
- playground 데이터 초기화: `cd member-dragon/playground && ./reset.sh`
- 테스트 기준과 상세 규약: `common/docs/`, `member-dragon/harness-starter-pack/`
