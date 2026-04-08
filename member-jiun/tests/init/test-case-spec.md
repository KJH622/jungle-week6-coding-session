# 초기화 테스트 케이스 명세

이 문서는 `run_init_tests.sh`에서 실행하는 초기화(Initialization) 테스트를 설명합니다.

## 1) `init_with_required_schema_success`
- 시나리오:
  - 작업 디렉토리에 `users.schema`, `products.schema`를 준비합니다.
  - SQL 파일은 `SELECT * FROM users;` 한 문장으로 실행합니다.
- 의도:
  - 프로그램 시작 시 필수 스키마를 읽고 초기화가 정상 완료되는지 확인합니다.
  - 초기화가 끝난 뒤 기본 조회 루프까지 무에러로 진입 가능한지 확인합니다.
- 검사:
  - 종료코드 `0`
  - `stdout` 비어 있음 (빈 테이블 조회)
  - `stderr` 비어 있음

## 2) `init_missing_users_schema_should_fail`
- 시나리오:
  - `products.schema`만 존재하고 `users.schema`는 없음
  - SQL은 `SELECT * FROM users;`
- 의도:
  - 필수 스키마 누락 시 초기화 단계에서 실패하는지 확인합니다.
- 검사:
  - 종료코드 비정상(`!= 0`)
  - `stdout` 비어 있음
  - `stderr` 비어 있지 않음

## 3) `init_missing_products_schema_should_fail`
- 시나리오:
  - `users.schema`만 존재하고 `products.schema`는 없음
  - SQL은 `SELECT * FROM users;`
- 의도:
  - 필수 스키마 누락 시 초기화 단계에서 실패하는지 확인합니다.
- 검사:
  - 종료코드 비정상(`!= 0`)
  - `stdout` 비어 있음
  - `stderr` 비어 있지 않음

## 4) `init_malformed_schema_should_fail`
- 시나리오:
  - `users.schema`를 손상된 JSON으로 제공
  - `products.schema`는 정상
- 의도:
  - 스키마 파싱 오류를 초기화 단계에서 감지하는지 확인합니다.
- 검사:
  - 종료코드 비정상(`!= 0`)
  - `stdout` 비어 있음
  - `stderr` 비어 있지 않음

## 5) `init_unreadable_schema_should_fail`
- 시나리오:
  - `users.schema` 읽기 권한 제거(`chmod 000`)
  - `products.schema`는 정상
- 의도:
  - 파일 접근 권한 문제를 초기화 단계에서 감지하는지 확인합니다.
- 검사:
  - 종료코드 비정상(`!= 0`)
  - `stdout` 비어 있음
  - `stderr` 비어 있지 않음
