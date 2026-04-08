# 함수 흐름 요약

이 문서는 `src` 내부 함수들이 큰 맥락에서 어떻게 이어지는지 간결하게 정리합니다.

## 전체 실행 흐름
1. `main`
: 프로그램 시작점입니다. 실제 작업은 `run_cli`로 바로 넘깁니다.

2. `run_cli`
: CLI 인자 개수를 확인합니다. 인자가 잘못되면 `ERROR: file open failed`를 출력하고 종료합니다. 정상 입력이면 `process_sql_file`을 호출합니다.

3. `process_sql_file`
: 전체 실행을 조합하는 중심 함수입니다.
: 입력 파일을 읽고, SQL 문장으로 분리하고, 스키마를 로드한 뒤, 각 문장을 순서대로 실행합니다.
: 마지막에는 에러 발생 여부를 모아 종료코드 `0/1`을 결정합니다.

## 입력 처리 흐름
1. `read_entire_file`
: SQL 파일 전체를 메모리로 읽습니다.

2. `split_statements`
: 파일 내용을 `;` 기준으로 문장 단위로 나눕니다.
: 이때 빈 문장, 공백만 있는 문장, 따옴표 내부 `;`를 구분해 처리합니다.

## 스키마 처리 흐름
1. `load_required_schemas`
: `users.schema`, `products.schema`를 읽어 테이블 메타데이터를 준비합니다.

2. `find_table`
: 테이블 이름으로 스키마를 찾습니다.

3. `find_col_index`
: 컬럼 이름으로 컬럼 인덱스를 찾습니다.

## INSERT 흐름
1. `is_insert_stmt`
: 현재 문장이 `INSERT`인지 빠르게 판별합니다.

2. `parse_insert`
: `INSERT` 문장을 해석합니다.
: 테이블 존재 여부, 컬럼 목록, 값 목록, 컬럼 수/값 수 일치 여부, 정수 타입 형식을 검증합니다.
: 성공하면 `InsertPlan`을 만듭니다.

3. `execute_insert`
: `InsertPlan`을 실제 파일 저장으로 실행합니다.
: 각 값을 인코딩해 `<table>.data` 파일에 append 합니다.

## SELECT 흐름
1. `is_select_stmt`
: 현재 문장이 `SELECT`인지 빠르게 판별합니다.

2. `parse_select`
: `SELECT` 문장을 해석합니다.
: `*` 조회인지, 특정 컬럼 조회인지 판단하고, 테이블/컬럼 유효성을 검증합니다.
: 성공하면 `SelectPlan`을 만듭니다.

3. `execute_select`
: `SelectPlan`을 실제 조회로 실행합니다.
: `<table>.data` 파일을 읽고, 헤더와 데이터 행을 CSV 형식으로 출력합니다.

## 공통 유틸 함수
1. `print_error`
: 모든 표준 에러 출력을 `ERROR: <message>` 형식으로 통일합니다.

2. `strvec_*`
: SQL 문장 목록처럼 길이가 가변적인 문자열 리스트를 관리합니다.

3. `text.*`
: 공백 제거, 키워드 비교, 식별자 파싱, 정수 판별 같은 문자열 처리 공통 기능을 담당합니다.

## 레이어 관점에서 보면
1. `interfaces`
: 바깥 입력을 받습니다.

2. `application`
: 전체 흐름을 조합합니다.

3. `domain`
: SQL 규칙과 검증을 담당합니다.

4. `infrastructure`
: 실제 파일 입출력과 저장을 담당합니다.

5. `shared`
: 여러 레이어가 함께 쓰는 공통 도구를 제공합니다.
