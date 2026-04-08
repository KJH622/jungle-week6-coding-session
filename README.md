# SQL 처리기 — 수요코딩회

## 프로젝트 개요
C 언어로 구현하는 파일 기반 SQL 처리기.
텍스트 파일의 SQL문을 파싱하고 실행하여, 파일 기반으로 데이터를 저장/조회한다.

## 진행 방식
이 프로젝트는 **같은 기획서를 각자 독립 구현한 뒤, 비교하고 합치는** 방식으로 진행했다.

| Phase | 구분 | 내용 | 시간 |
|-------|------|------|------|
| 1 | 공통 | 도메인 학습 + 테스트 설계 + 입출력 합의 | 11:00~14:00 |
| 2 | 개인 | 각자 바이브코딩으로 전체 구현 | 14:00~18:00 |
| 3 | 공통 | 발표 + 히든 테스트 + 비교 토론 | 19:00~20:30 |
| 4 | 공통 | 베스트 선정 + 통합 완성 | 20:30~22:00 |
| 5 | 공통 | 코드 학습 + 정리 | 22:00~23:00 |

## 빌드 및 실행
```bash
cd final/src
make
./sql_processor input.sql
```

## 테스트
```bash
# 공개 테스트
./common/scripts/run_tests.sh ./final/src/sql_processor public

# 전체 테스트 (히든 포함)
./common/scripts/run_tests.sh ./final/src/sql_processor hidden
```

## 팀원별 구현
각 팀원의 구현과 산출물은 아래 폴더에서 확인 가능.
- `jihyun/`
- `hyeonok/`
- `dragon/`
- `jiun/`
- `prompts.md` — AI 활용 이력
- `decisions.md` — 설계 판단 기록
- `trouble.md` — 삽질 및 해결 과정
