# 에러 메시지 표준

SQL 처리기에서 stderr 출력 시 아래 형식을 사용한다.

- 형식: `ERROR: <message>`

## 표준 메시지 목록
- `ERROR: invalid query`
- `ERROR: table not found`
- `ERROR: column count does not match value count`
- `ERROR: file open failed`

## 사용 가이드
- 문구는 대소문자/철자를 동일하게 유지한다.
- 추가 설명(테이블명, 컬럼명 등)은 현재 표준 범위에서는 붙이지 않는다.
- 한 SQL 문장에서 여러 에러를 출력하지 않고, 해당 문장의 대표 에러 1개만 출력한다.
