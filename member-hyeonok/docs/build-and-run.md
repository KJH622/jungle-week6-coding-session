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

## 현재 단계 구현 범위

- SQL 파일 전체 읽기
- 세미콜론 기준 SQL 문장 분리
- 빈 문장 제거
- parser / executor 기본 인터페이스 연결

## 다음 단계 예정

- INSERT / SELECT 실제 파싱
- JSON 스키마 읽기
- 파일 기반 저장 및 조회 실행
- 표준 에러 처리 세분화
