# member-hyeonok SQL Processor 빌드/실행 메모

## 빌드

```bash
cd /Users/ok/Downloads/jungle_project/jungle-week6-coding-session/member-hyeonok/src
make
```

## 실행

```bash
./sql_processor input.sql
```

## 현재 구현 범위

- SQL 파일 전체 읽기
- 세미콜론 기준 SQL 문장 분리
- 빈 문장 무시
- `INSERT INTO <table> (col1, col2, ...) VALUES (val1, val2, ...)`
- `SELECT * FROM <table>`
- `SELECT col1, col2, ... FROM <table>`
- JSON 기반 `<table>.schema` 로딩
- `<table>.data` 파일 기반 저장/조회
- 표준 에러 메시지 출력

## 현재 구현하지 않은 범위

- `CREATE TABLE`
- `WHERE`, `ORDER BY`, 함수 호출, 표현식
- 범용 JSON 파싱

## 확인한 테스트 상태

- 공개 테스트: 통과
- hidden 테스트: 통과
