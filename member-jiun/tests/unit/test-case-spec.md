# 단위 테스트 케이스 명세

이 문서는 `run_unit_tests.sh`에서 실행하는 함수 단위 테스트를 설명합니다.

## 검증 범위
1. `shared/text`
: 대소문자 비교, 정수 리터럴 판별 같은 기초 문자열 유틸을 검증합니다.

2. `domain/parser`
: 문장 분리, `INSERT` 파싱, `SELECT` 파싱의 핵심 분기와 에러 메시지를 검증합니다.

## 포함 케이스
1. `ci_equal`, `ci_n_equal`, `is_integer_literal` 정상/실패 케이스
2. `split_statements`가 빈 문장과 따옴표 내부 `;`를 올바르게 처리하는지 확인
3. `parse_insert` 성공 케이스
4. `parse_insert` 컬럼/값 개수 불일치 에러
5. `parse_insert` 존재하지 않는 테이블 에러
6. `parse_select` 특정 컬럼 선택 성공 케이스
7. `parse_select` 존재하지 않는 컬럼 에러
8. `parse_select` 존재하지 않는 테이블 에러
