# 제출 전 확인 문서

## 1. 프로젝트 파일 구조 설명

```text
member-hyeonok/
  docs/
    build-and-run.md
    submission-checklist.md
  src/
    Makefile
    main.c
    parser.c / parser.h
    sql_splitter.c / sql_splitter.h
    schema.c / schema.h
    storage.c / storage.h
    executor.c / executor.h
    error.c / error.h
    util.c / util.h
    ast.h
```

- `main.c`: SQL 파일 전체 처리, 문장별 실행, 종료코드 결정
- `sql_splitter.c`: 세미콜론 기준 문장 분리, 빈 문장 제거
- `parser.c`: `INSERT`, `SELECT` 파싱 및 AST 생성
- `schema.c`: `<table>.schema` JSON 로딩
- `storage.c`: `<table>.data` 파일 append/read/행 파싱
- `executor.c`: `INSERT` 저장, `SELECT` 조회/출력
- `error.c`: 공통 에러 코드와 stderr 메시지 출력
- `util.c`: 공용 문자열/파일 읽기 헬퍼

## 2. 빌드 방법

```bash
cd /Users/ok/Downloads/jungle_project/jungle-week6-coding-session/member-hyeonok/src
make
```

## 3. 실행 방법

```bash
./sql_processor input.sql
```

## 4. 지원 SQL 문법

- `INSERT INTO users (name, age, major) VALUES ('김민준', 25, '컴퓨터공학')`
- `SELECT * FROM users`
- `SELECT name, major FROM users`

지원 범위 메모:
- SQL 키워드는 대소문자를 구분하지 않음
- 공백이 많아도 파싱 가능
- 여러 SQL 문장을 한 파일에서 순차 실행 가능
- 빈 문장은 무시

미구현:
- `CREATE TABLE`
- `WHERE`
- `ORDER BY`
- 함수 호출/표현식

## 5. 에러 규칙

stderr로만 아래 문구를 정확히 출력한다.

- `ERROR: invalid query`
- `ERROR: table not found`
- `ERROR: column count does not match value count`
- `ERROR: file open failed`

규칙:
- 한 SQL 문장에 대표 에러 1개만 출력
- 추가 설명을 붙이지 않음
- 에러가 나도 다음 SQL 문장은 계속 실행
- 하나라도 실패하면 최종 종료코드 `1`
- 전부 성공하면 최종 종료코드 `0`

## 6. 공개 테스트 체크리스트

- `INSERT` 성공 시 stdout 출력이 없는가
- `SELECT *`가 스키마 순서대로 출력되는가
- `SELECT col1, col2`가 요청 순서대로 출력되는가
- 헤더와 데이터가 모두 쉼표 구분인지 확인했는가
- SQL 키워드 대소문자 혼용 입력을 통과하는가
- 여러 SQL 문장을 순차 실행하는가
- 한 테이블에 여러 번 `INSERT` 후 누적 조회가 되는가
- `users`와 `products`를 동시에 처리할 수 있는가

## 7. hidden test에서 주의할 포인트

- 빈 데이터 파일 또는 데이터 파일 미존재 시 `SELECT`는 무출력이어야 함
- 존재하지 않는 테이블은 `ERROR: table not found`
- 잘못된 컬럼 선택은 `ERROR: invalid query`
- 값 내부 공백을 그대로 보존해야 함
- 값 내부 쉼표가 있어도 저장/조회가 깨지지 않아야 함
- 컬럼 수와 값 수가 다르면 파일에 쓰지 않아야 함
- 에러가 난 문장 뒤의 다음 문장은 계속 실행돼야 함

## 8. 실수하기 쉬운 부분

- `common/` 경로를 직접 참조하지 말 것
- schema/data 파일은 현재 작업 디렉토리 기준 상대 경로로 찾아야 함
- hidden 테스트는 임시 디렉토리에서 실행되므로 절대경로 가정이 깨짐
- 출력 마지막 줄 개행이 빠지지 않게 할 것
- stderr에 설명을 더 붙이지 말 것
- `SELECT *`에서 빈 테이블일 때 헤더를 출력하면 실패함

## 9. 이 구현이 왜 common 테스트 구조에 맞는지

- 테스트 러너는 실행 전에 `.schema` 파일을 임시 작업 디렉토리로 복사한다.
- 이 구현은 `<table>.schema`, `<table>.data`를 현재 작업 디렉토리 기준으로 열기 때문에 임시 디렉토리 실행과 맞는다.
- SQL 파일도 인자로 받은 경로 그대로 열어서 테스트 러너의 복사 방식과 충돌하지 않는다.
- stdout은 조회 결과만, stderr는 표준 에러만 출력해서 `diff` 기반 비교와 맞는다.

## 10. 제출 전 최종 체크리스트

- `member-hyeonok/src`에서 `make`가 성공하는가
- `./common/scripts/run_tests.sh ./member-hyeonok/src/sql_processor public`가 통과하는가
- `./common/scripts/run_tests.sh ./member-hyeonok/src/sql_processor hidden`가 통과하는가
- `common/` 폴더를 수정하지 않았는가
- `member-hyeonok/` 밖을 수정하지 않았는가
- 문서 내용이 실제 구현과 일치하는가
- 불필요한 디버그 출력이 없는가
