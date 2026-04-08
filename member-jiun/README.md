# SQL 처리기

파일 기반으로 동작하는 작은 SQL 처리기입니다.  
입력 SQL 파일을 읽어서 `INSERT`, `SELECT`를 실행하고, 결과를 파일에 저장하거나 조회합니다.

## 목표

이 프로젝트의 목표는 SQL 문장을 읽고, 파싱하고, 실행하고, 파일에 저장하는 전체 흐름을 직접 구현하는 것입니다.  
이번 구현에서는 공개 테스트 정합성과 설명 가능성을 우선해, 범위를 명확히 제한하고 구조를 모듈화했습니다.

## 지원 범위

1. `INSERT INTO ... VALUES (...)`
2. `SELECT * FROM ...`
3. `SELECT col1, col2 FROM ...`
4. 키워드 대소문자 비구분
5. 에러 발생 후 다음 SQL 계속 실행

구현하지 않은 범위입니다.
1. `CREATE TABLE`
2. `WHERE`
3. `UPDATE`, `DELETE`
4. JOIN, 정렬, 집계

## 실행 방법

```bash
cd member-jiun/src
make
./sql_processor input.sql
```

## 예시

```sql
INSERT INTO users (name, age, major) VALUES ('김민준', 25, '컴퓨터공학');
INSERT INTO users (name, age, major) VALUES ('이서연', 22, '경영학');
SELECT name, major FROM users;
```

출력 예시입니다.

```text
name,major
김민준,컴퓨터공학
이서연,경영학
```

## 설계 요약

현재 구현은 클린 아키텍처 형태로 나누어져 있습니다.

1. `interfaces`
: CLI 입력과 진입점을 담당합니다.

2. `application`
: 전체 실행 흐름과 종료코드 집계를 담당합니다.

3. `domain`
: SQL 파싱, 스키마 검증, 규칙 처리를 담당합니다.

4. `infrastructure`
: 파일 읽기/쓰기와 데이터 저장을 담당합니다.

5. `shared`
: 문자열 처리, 벡터, 에러 출력 등 공통 유틸을 담당합니다.

## 데이터 저장 방식

각 테이블은 `<table>.data` 파일로 관리합니다.  
내부적으로는 탭 구분 기반의 커스텀 텍스트 포맷을 사용하고, 특수문자는 이스케이프해서 저장합니다.

이 방식을 선택한 이유입니다.
1. 구현이 단순합니다.
2. 디버깅이 쉽습니다.
3. 현재 과제 범위에 맞는 복잡도로 유지할 수 있습니다.

## 테스트

### 단위 테스트

```bash
./member-jiun/tests/unit/run_unit_tests.sh
```

### 기능 테스트

```bash
./member-jiun/tests/cli/run_cli_tests.sh ./member-jiun/src/sql_processor
./member-jiun/tests/init/run_init_tests.sh ./member-jiun/src/sql_processor
./member-jiun/tests/input/run_input_tests.sh ./member-jiun/src/sql_processor
./member-jiun/tests/parse/run_parse_tests.sh ./member-jiun/src/sql_processor
./member-jiun/tests/insert/run_insert_tests.sh ./member-jiun/src/sql_processor
./member-jiun/tests/select/run_select_tests.sh ./member-jiun/src/sql_processor
./member-jiun/tests/exit/run_exit_tests.sh ./member-jiun/src/sql_processor
```

### 공개 테스트

```bash
./common/scripts/run_tests.sh ./member-jiun/src/sql_processor public
```

## 검증 포인트

이번 구현에서 특히 신경 쓴 부분입니다.

1. 입력 파일/인자 예외 처리
2. 빈 줄, 공백, 멀티라인, 마지막 세미콜론 누락 처리
3. 문자열 내부 쉼표/세미콜론 보존
4. 에러 발생 후 다음 문장 계속 실행
5. 종료코드 집계 정확성

## 문서

추가 문서입니다.

1. [설계 판단 기록](/Users/nako/jungle/wed_coding_session/jungle-week6-coding-session/member-jiun/decisions.md)
2. [AI 활용 이력](/Users/nako/jungle/wed_coding_session/jungle-week6-coding-session/member-jiun/prompts.md)
3. [서비스 흐름](/Users/nako/jungle/wed_coding_session/jungle-week6-coding-session/member-jiun/docs/flow.md)
4. [코드 실행 흐름](/Users/nako/jungle/wed_coding_session/jungle-week6-coding-session/member-jiun/docs/code-flow.md)
5. [함수 흐름 요약](/Users/nako/jungle/wed_coding_session/jungle-week6-coding-session/member-jiun/src/function-flow.md)
6. [체크리스트](/Users/nako/jungle/wed_coding_session/jungle-week6-coding-session/member-jiun/docs/checklist.md)
