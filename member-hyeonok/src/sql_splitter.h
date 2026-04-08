#ifndef SQL_SPLITTER_H
#define SQL_SPLITTER_H

#include <stddef.h>

typedef struct SqlStatementList {
    char **items;
    size_t count;
} SqlStatementList;

/*
 * SQL 파일 전체 문자열을 세미콜론 기준으로 분리한다.
 * 비어 있는 문장은 제거하며, 문자열 리터럴 내부의 세미콜론은 분리 기준으로 보지 않는다.
 */
int split_sql_statements(const char *sql_text, SqlStatementList *list);

/*
 * split_sql_statements 로 생성한 결과를 정리한다.
 */
void free_sql_statement_list(SqlStatementList *list);

#endif
