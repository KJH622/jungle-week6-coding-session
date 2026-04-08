#include "include/status.h"

/* 내부 상태 코드를 사용자에게 보여줄 에러 메시지 문자열로 변환한다. */
const char *sql_status_message(SqlStatus status)
{
    switch (status) {
        case SQL_STATUS_OK:
            return "";
        case SQL_STATUS_INVALID_QUERY:
            return "invalid query";
        case SQL_STATUS_TABLE_NOT_FOUND:
            return "table not found";
        case SQL_STATUS_COLUMN_VALUE_COUNT_MISMATCH:
            return "column count does not match value count";
        case SQL_STATUS_FILE_OPEN_FAILED:
            return "file open failed";
        case SQL_STATUS_INTERNAL_ERROR:
        default:
            return "invalid query";
    }
}
