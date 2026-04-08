# 입출력 형식 명세 (팀 합의용)

## 실행 형식

```bash
./sql_processor <sql_file>
```

## 출력 규칙

### INSERT
- 성공 시: 출력 없음 (silent)
- 실패 시: stderr로 에러 메시지

### SELECT
- 첫 줄: 컬럼 헤더 (쉼표 구분)
- 이후: 데이터 행 (쉼표 구분)
- 행 끝: 개행문자 (`\n`)
- 데이터가 없는 경우: 헤더만 출력

```
name,age,major
김민준,25,컴퓨터공학
이서연,22,경영학
```

### 에러 출력
- stderr로 출력
- 형식: `[ERROR] 메시지`
- 에러 발생 후에도 다음 SQL문은 계속 실행

## 스키마 정의

테이블은 미리 존재한다고 가정하므로, 스키마 파일로 정의한다.
각 테이블의 스키마는 `common/schema/` 디렉토리에 위치한다.

```
# schema/users.schema
name:string
age:int
major:string
```

```
# schema/products.schema
name:string
price:int
category:string
```

## 종료 코드
- 모든 SQL 정상 실행: 0
- 하나라도 에러 발생: 1
