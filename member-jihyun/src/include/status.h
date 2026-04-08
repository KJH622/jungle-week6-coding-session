#ifndef STATUS_H
#define STATUS_H

typedef enum SqlStatus {
    SQL_STATUS_OK = 0,
    SQL_STATUS_INVALID_QUERY,
    SQL_STATUS_TABLE_NOT_FOUND,
    SQL_STATUS_COLUMN_VALUE_COUNT_MISMATCH,
    SQL_STATUS_FILE_OPEN_FAILED,
    SQL_STATUS_INTERNAL_ERROR
} SqlStatus;

const char *sql_status_message(SqlStatus status);

#endif
