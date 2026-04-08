# Active Execution Plan

## Task

`member-dragon/src/`의 파일 기반 SQL 처리기를 Phase 2 범위까지 확장하고, 하네스 문서/로그도 함께 최신화한다.

## Scope

- 바꿀 것: `member-dragon/src/` 구현, `member-dragon/plan_codex.md`, `member-dragon/usage.md`, 개인 기록 문서, 하네스 상태 문서/로그
- 바꾸지 않을 것: `common/` 아래 테스트/스키마/스크립트, 범위 밖 SQL 기능(`JOIN`, `GROUP BY`, `AND/OR`, 집계 함수)

## Stages

1. Discover
2. Plan
3. Implement
4. Verify
5. Review

## Current Status

- Current stage: `review`
- Approval required: `false`
- Next step: `Summarize the completed Phase 2 implementation and hand off the updated docs`

## Artifacts

- spec: `member-dragon/plan_codex.md`, `member-dragon/harness-starter-pack/docs/product-specs/feature-template.md`
- design: `member-dragon/harness-starter-pack/ARCHITECTURE.md`, `member-dragon/harness-starter-pack/docs/design-docs/domain-rules.md`
- usage: `member-dragon/usage.md`
- review notes: `member-dragon/decisions.md`, `member-dragon/trouble.md`
- test output:
  - `make clean && make`
  - public 6/6 PASS via `common/scripts/run_tests.sh`
  - hidden 8/8 PASS via `common/scripts/run_tests.sh`
  - manual extended scenario PASS for `WHERE`, `ORDER BY`, `LIMIT`, `UPDATE`, `DELETE`, reordered `INSERT`

## Review Notes

- 기존 확장 계획의 가장 큰 위험은 `WHERE`와 `ORDER BY`를 projection 이후에 적용하려던 점이었다.
- 이를 막기 위해 `StorageOps`를 `read_all_rows` / `replace_rows`로 additively 확장하고, executor가 full-row 기준 파이프라인을 담당하도록 수정했다.
- `INSERT`는 schema 전체 컬럼을 모두 요구하되, 컬럼 순서가 달라도 재정렬해 저장하도록 확장했다.

## Residual Risks

- 고정 크기 버퍼 기반이라 아주 큰 입력은 현재 범위 밖이다.
- `WHERE`는 단일 조건만 지원하며 `AND`, `OR`, 괄호는 지원하지 않는다.
- `DELETE`/`UPDATE`는 파일 전체를 다시 쓰는 방식이라 대용량 데이터에는 비효율적일 수 있다.
- 데이터 파일 포맷은 프로젝트 전용 custom encoding이다.
