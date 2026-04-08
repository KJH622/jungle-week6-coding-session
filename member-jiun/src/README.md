# src 구조 (Clean Architecture)

- `interfaces/`: CLI 입출력, 프로그램 엔트리와 가까운 어댑터
- `application/`: 유스케이스 조합(쿼리 실행 흐름)
- `domain/`: 핵심 규칙(SQL 명령 모델, 검증 규칙)
- `infrastructure/`: 파일 저장소, 스키마 로더 등 외부 의존 구현
- `shared/`: 공통 유틸/에러 타입/상수

의존 방향은 바깥(`interfaces`, `infrastructure`)에서 안쪽(`application`, `domain`)으로만 향합니다.
