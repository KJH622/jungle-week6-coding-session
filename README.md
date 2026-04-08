# SQL Processor — C로 만든 파일 기반 SQL 처리기

> 수요코딩회 Week 07 | 팀원: dragon, hyeonok, jihyun, jiun

---

## 이 프로젝트가 특별한 이유

이 프로젝트는 하나의 결과물을 분업한 게 아닙니다.
**같은 기획서를 각자 독립 구현하고, 비교하고, 베스트를 골라 합쳤습니다.**
 
왜 이렇게 했는가?
- AI가 코드를 다 짜주는 시대에, 기능 하나씩 나눠서 붙이면 "AI한테 시키고 끝"이 됩니다
- 같은 문제를 다르게 풀어야 "왜 달라졌는지"를 비교할 수 있고, 그게 진짜 학습입니다
- 4명이 4가지 접근법을 경험하는 효과를 하루 만에 얻었습니다

---

## 데모

### 웹 UI
브라우저에서 SQL을 직접 입력하고 실행 결과를 확인할 수 있습니다.

![데모사진](assets/demo-ui.png)

### CLI
```bash
$ ./sql_processor demo.sql
name,age,major
김민준,25,컴퓨터공학
이서연,22,경영학
```

---

## 빌드 및 실행

```bash
cd member-jihyun/src
make
./sql_processor input.sql
```

---

## 아키텍처

![아키텍처](assets/architecture-team.svg)

### 모듈 구성

| 파일 | 역할 |
|------|------|
| `main.c` | CLI 인자 확인, 파일 읽기, parse/execute 루프 |
| `parser.c` | SQL 분리 및 INSERT/SELECT 파싱 |
| `ast.c` | AST 구조체 생성/해제 |
| `executor.c` | AST 실행, 컬럼/테이블 검증, 출력 |
| `schema.c` | .schema 파일 로드/해석 |
| `storage.c` | row 직렬화/역직렬화, 테이블 파일 접근 |
| `page.c` | 4096바이트 페이지 포맷/레코드 관리 |
| `buffer_cache.c` | 페이지 캐시 및 flush |
| `status.c`, `utils.c` | 상태 문자열/문자열 유틸 |

---

## 설계 판단

| 판단 항목 | 결정 | 근거 |
|-----------|------|------|
| 저장 포맷 | 바이너리 + 4096B Page | CSV보다 쉼표/확장성 대응에 유리 |
| 파서 방식 | split → parse → AST | 문장 분리와 파싱 책임 분리로 확장성 확보 |
| 에러 후 동작 | 에러 SQL만 출력 후 계속 실행 | MySQL, SQLite 동작 참고 |
| 빈 테이블 SELECT | 출력 없음 | SQLite 동작 참고 |
| INSERT 성공 출력 | 별도 메시지 없음 | stdout은 SELECT 결과 중심 |
| 스키마 관리 | .schema 파일 로드 | CREATE TABLE 미구현 조건 충족 |

---

## 테스트

### 자동화 테스트

```bash
# 공개 테스트
./common/scripts/run_tests.sh ./member-jihyun/src/sql_processor public

# 히든 테스트
./common/scripts/run_tests.sh ./member-jihyun/src/sql_processor hidden
```

### 결과

- public: **6/6 통과**
- hidden: **8/8 통과**

### 즉석 스트레스 테스트

| 케이스 | 결과 | 비고 |
|--------|------|------|
| 세미콜론 없는 SQL | 통과 | 마지막 문장도 처리됨 |
| 빈 문자열 값 `''` | 통과 | 빈 문자열 insert 가능 |
| SQL 키워드가 값에 포함 | 통과 | 문자열 값으로 처리 |
| 음수 값 | 통과 | int 파싱 허용 |
| 아주 긴 문자열 | 통과 | 정상 insert |
| 같은 테이블 SELECT 3회 | 통과 | 연속 조회 정상 |
| 완전히 잘못된 SQL | 통과 | 에러 출력 후 계속 진행 |
| 빈 파일 | 통과 | 종료코드 0 |
| 이스케이프된 따옴표 `'O''Reilly'` | jihyun, jiun 통과 / dragon, hyeonok 실패 | |

---

## 팀원별 구현 비교

| 비교 항목 | dragon | hyeonok | jihyun | jiun |
|-----------|--------|---------|--------|------|
| 저장 포맷 | length-prefixed 텍스트 | 탭 구분 + escape 텍스트 | 바이너리 page 기반 | custom escape 텍스트 |
| 파서 방식 | 수동 파싱 (keyword scan) | 수동 파싱 + sql_splitter | 수동 파싱 + AST + quote-aware split | 수동 파싱 + quote-aware split |
| 대소문자 처리 | strncasecmp | tolower 기반 | util_case_* 유틸 | ci_equal / ci_n_equal |
| 파일 수 | 11 | 16 | 19 | 23 |
| 코드 줄 수 | 2256 | 2151 | 2250 | 1642 |
| 함수 수 | 38 | 61 | 76 | 35 |
| 최장 함수 | 91줄 | 114줄 | 84줄 | 169줄 |
| AI 활용 방식 | 하네스 + 병렬 에이전트 | 단계별 구현 요청 | 단계별 검증 + 체크리스트 | 요구사항 잠금 + 테스트 우선 |
| 대표 삽질 | 초기 SELECT segfault | 문자열 trim/저장포맷 충돌 | 환경/검증 입력 불일치 | 단일 파일 비대화 후 모듈 분리 |

### 핵심 차이점

- **dragon**: 파일 저장소 추상화(vtable)와 custom row 포맷
- **hyeonok**: 에러 처리 중앙화와 storage 분리 리팩터링
- **jihyun**: AST + page + buffer_cache까지 포함한 확장형 구조
- **jiun**: 요구사항 잠금과 테스트 우선 흐름 기반 개발

---

## AI 활용 회고

### 공통으로 확인한 점

**AI가 잘한 점**
- 보일러플레이트 생성, 모듈 분리, 반복 리팩터링 속도
- 테스트 스크립트/문서 자동화

**AI가 자주 틀린 점**
- 문자열 파싱의 경계 조건 (quote, escape, comma, semicolon)
- 요구사항 문구와 실제 코드 동작 불일치

**효과적이었던 프롬프트 패턴**
- "한 번에 전부 말고 단계별로 진행"
- "테스트 먼저 만들고 구현"
- "결정 근거를 문서로 남기기"

**비효과적이었던 프롬프트 패턴**
- "전체 코드 한 번에 만들어줘"
- "검증 없이 바로 기능 추가"

### 핵심 교훈

> AI는 구현 속도를 크게 높여주지만, 요구사항 잠금/검증 기준/설계 판단은 사람이 선행해야 품질이 안정된다.

---

## 추가 구현

- [x] 파일 기반 SQL Processor (INSERT, SELECT)
- [x] public/hidden 자동 테스트 전원 통과
- [x] quote-aware 파싱
- [x] page + buffer cache 내부 구조
- [x] 웹 UI 데모

---

## 레포 구조

```
jungle-week6-coding-session/
├── README.md
├── common/
│   ├── docs/
│   ├── schema/
│   ├── tests/
│   └── scripts/
├── member-dragon/
├── member-hyeonok/
├── member-jihyun/   ← 베스트 선정
└── member-jiun/
```