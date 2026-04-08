# IO Spec (초안)

## 실행
- `./sql_processor input.sql`

## 출력 규칙
- INSERT 성공: 출력 없음
- SELECT 성공: 첫 줄 헤더, 이후 데이터(쉼표 구분)
- 에러: stderr로 `[ERROR]` 메시지
