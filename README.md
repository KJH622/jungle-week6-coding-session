# SQL Processor — C로 만든 파일 기반 SQL 처리기

> 수요코딩회 Week 07 | 팀원: dragon, hyeonok, jihyun, jiun

---

## 이 프로젝트가 특별한 이유

이 프로젝트는 하나의 결과물을 분업한 것이 아니라,
**같은 기획서를 각자 독립 구현한 뒤 비교하고 베스트를 선정해 통합한 방식**으로 진행했습니다.

베스트 선정 결과는 **jihyun 구현**입니다.

---

## 데모 (Best: jihyun)

### 파일 모드 UI
```bash
$ ./sql_processor demo.sql
name,age,major
김민준,25,컴퓨터공학
이서연,22,경영학
```

- 현재 베스트 코드(`member-jihyun`)는 **파일 모드만 지원**합니다.
- 인자 없이 실행 시 인터랙티브 모드로 동작하지 않습니다.

---

## 빌드 및 실행

```bash
cd member-jihyun/src
make
./sql_processor input.sql
```

---

## 아키텍처 (jihyun 기준)

![팀 아키텍처](assets/architecture-team.svg)

### 모듈 구성

| 파일 | 역할 |
|------|------|
| `main.c` | CLI 인자 확인, 파일 읽기, parse/execute 루프, 종료코드 결정 |
| `parser.c` | SQL 분리 및 `INSERT`/`SELECT` 파싱 |
| `ast.c` + `include/ast.h` | AST 구조체 생성/해제 |
| `executor.c` | AST 실행, 컬럼/테이블 검증, 출력 제어 |
| `schema.c` | `.schema` 로드/해석 |
| `storage.c` | row 직렬화/역직렬화, 테이블 파일 접근 |
| `page.c` | 4096바이트 페이지 포맷/레코드 append/scan |
| `buffer_cache.c` | 페이지 캐시 및 flush |
| `status.c`, `utils.c` | 상태 문자열/문자열 유틸 |

---

## 우리가 내린 설계 판단들 (jihyun 기준)

| 판단 항목 | 결정 | 근거 |
|-----------|------|------|
| 저장 포맷 | `<table>.data` 바이너리 + 4096B Page | `member-jihyun/decisions.md`에 명시. CSV보다 쉼표/확장성 대응에 유리 |
| 파서 방식 | split -> parse -> AST (수동 파싱) | 문장 분리와 파싱 책임 분리로 확장성/설명성 확보 |
| 에러 후 동작 | 에러 SQL만 출력 후 다음 SQL 계속 | `main.c` 루프에서 continue 처리, 최종 had_error 집계 |
| 빈 테이블 SELECT | 출력 없음 | `decisions.md`와 실제 테스트 동작 기준 |
| INSERT 성공 출력 | 파일 모드에서는 별도 성공 메시지 없음 | stdout은 SELECT 결과 중심 |
| 스키마 관리 | `<table>.schema` 파일 로드 | CREATE TABLE 미구현 조건 충족 |

---

## 테스트

### 자동화 테스트

```bash
# 공개 테스트
./common/scripts/run_tests.sh ./member-jihyun/src/sql_processor public

# 히든 테스트
./common/scripts/run_tests.sh ./member-jihyun/src/sql_processor hidden
```

### 결과 (현재 코드 기준)

- public: **6/6 통과**
- hidden: **8/8 통과**

### 즉석 스트레스 테스트 (요청 케이스)

| 케이스 | 결과 | 비고 |
|--------|------|------|
| 세미콜론 없는 SQL | 통과 | 마지막 문장도 처리됨 |
| 빈 문자열 값 `''` | 통과 | 빈 문자열 insert 가능 |
| SQL 키워드가 값에 포함 | 통과 | 문자열 값으로 처리 |
| 음수 값 | 통과 | int 파싱 허용 |
| 아주 긴 문자열 | 통과 | 정상 insert |
| 같은 테이블 SELECT 3회 | 통과 | 연속 조회 정상 |
| 완전히 잘못된 SQL | 통과 | `ERROR: invalid query` 후 계속 진행 |
| 빈 파일 | 통과 | 종료코드 0 |

추가 비교 케이스(`'O''Reilly'`)에서는 `jihyun`, `jiun` 통과 / `dragon`, `hyeonok` 실패를 확인했습니다.

---

## 팀원별 구현 비교 (사실 기반)

| 비교 항목 | dragon | hyeonok | jihyun (best) | jiun |
|-----------|--------|---------|---------------|------|
| 저장 포맷 | custom length-prefixed 텍스트(`.data`) | 탭 구분 + escape 텍스트(`.data`) | 바이너리 page 기반(`.data`) | custom escape 텍스트(`.data`) |
| 파서 방식 | 수동 파싱 (keyword scan) | 수동 파싱 + `sql_splitter` | 수동 파싱 + AST + quote-aware split | 수동 파싱 + quote-aware split |
| `strtok` 사용 | SQL 파싱에는 미사용 | SQL 파싱에는 미사용 | SQL 파싱에는 미사용 | SQL 파싱 미사용(저장소 분해에 `strtok_r` 사용) |
| 대소문자 처리 | `strncasecmp` 기반 | `tolower` 기반 keyword match | `util_case_*` 유틸 기반 | `ci_equal`/`ci_n_equal` 기반 |
| 파일 수(`.c/.h`) | 11 | 16 | 19 | 23 |
| 코드 줄 수(`.c+.h`) | 2256 | 2151 | 2250 | 1642 |
| 함수 수 | 38 | 61 | 76 | 35 |
| 최장 함수 | 91줄 (`file_select_rows`) | 114줄 (`execute_select_statement`) | 84줄 (`schema_load`) | 169줄 (`parse_insert`) |
| AI 활용 방식 | 하네스 + 병렬 에이전트 포함 | 단계별 구현 요청 | 단계별 검증/체크리스트 중심 | 요구사항 잠금 + 테스트 우선 |
| 대표 삽질 | 초기 SELECT segfault(스택/힙) | 문자열 trim/저장포맷 충돌 | 환경/검증 입력 불일치 | 단일 파일 비대화 후 모듈 분리 |

---

## 팀원별 핵심 차이점

- **dragon**: 파일 저장소 추상화(vtable)와 custom row 포맷에 강점이 있습니다.
- **hyeonok**: 에러 처리 중앙화(`error.c`)와 storage 분리 리팩터링이 깔끔합니다.
- **jihyun**: AST + page + buffer_cache까지 포함한 확장형 구조가 강점입니다.
- **jiun**: 요구사항 잠금과 테스트 우선 흐름, 인터랙티브 UX 확장이 강점입니다.

---

## AI 활용 회고

### 공통으로 확인한 점

**AI가 잘한 점**
- 보일러플레이트 생성, 모듈 분리, 반복 리팩터링 속도
- 테스트 스크립트/문서 자동화

**AI가 자주 틀린 점**
- 문자열 파싱의 경계 조건(quote/escape/comma/semicolon)
- 요구사항 문구와 실제 코드 동작 불일치

**효과적이었던 프롬프트 패턴**
- "한 번에 전부 말고 단계별로 진행"
- "테스트 먼저 만들고 구현"
- "결정 근거를 문서로 남기기"

**비효과적이었던 프롬프트 패턴**
- "전체 코드 한 번에 만들어줘"
- "검증 없이 바로 기능 추가"

### 핵심 교훈

AI는 구현 속도를 크게 높여주지만, **요구사항 잠금/검증 기준/설계 판단은 사람이 선행해야 품질이 안정됩니다.**

---

## 추가 구현 (현 상태)

- [x] 파일 기반 SQL Processor (`INSERT`, `SELECT`)
- [x] public/hidden 자동 테스트 통과
- [x] quote-aware 파싱(베스트 기준)
- [x] page + buffer cache 내부 구조 (jihyun)
- [ ] 인터랙티브 모드(readline, `.tables`, `.schema`) — 베스트(jihyun) 기준 미적용

---

## 레포 구조

```text
jungle-week6-coding-session/
├── README.md
├── common/
│   ├── docs/
│   ├── schema/
│   ├── tests/
│   └── scripts/
├── member-dragon/
├── member-hyeonok/
├── member-jihyun/   <- 베스트 선정 구현
└── member-jiun/
```
