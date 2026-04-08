#include "parser.h"

#include "util.h"

#include <stdlib.h>

int parse_sql_statement(const char *sql_text, SqlStatement *statement)
{
    if (sql_text == NULL || statement == NULL) {
        return 0;
    }

    /*
     * 다음 단계에서 INSERT / SELECT 세부 파싱을 붙일 수 있도록
     * 일단 원문을 안전하게 보관하는 최소 AST 형태를 만든다.
     */
    statement->type = SQL_STATEMENT_UNCLASSIFIED;
    statement->raw_sql = duplicate_string(sql_text);
    if (statement->raw_sql == NULL) {
        return 0;
    }

    return 1;
}

void free_sql_statement(SqlStatement *statement)
{
    if (statement == NULL) {
        return;
    }

    free(statement->raw_sql);
    statement->raw_sql = NULL;
    statement->type = SQL_STATEMENT_UNCLASSIFIED;
}
