#include "executor.h"
#include "parser.h"
#include "sql_splitter.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>

static int process_sql_file(const char *sql_path)
{
    char *sql_text;
    SqlStatementList statements;
    size_t index;
    int had_error;

    /* 입력 SQL 파일을 통째로 읽은 뒤, 한 번만 분리해서 순서대로 처리한다. */
    sql_text = read_entire_file(sql_path);
    if (sql_text == NULL) {
        fprintf(stderr, "ERROR: file open failed\n");
        return 1;
    }

    if (!split_sql_statements(sql_text, &statements)) {
        free(sql_text);
        fprintf(stderr, "ERROR: file open failed\n");
        return 1;
    }

    had_error = 0;

    for (index = 0; index < statements.count; index++) {
        SqlStatement statement;

        /*
         * 빈 문장은 splitter 단계에서 제거되므로,
         * 여기서는 실제 SQL 문장만 순차적으로 parser / executor 에 넘긴다.
         */
        if (!parse_sql_statement(statements.items[index], &statement)) {
            had_error = 1;
            continue;
        }

        if (!execute_sql_statement(&statement)) {
            had_error = 1;
        }

        free_sql_statement(&statement);
    }

    free_sql_statement_list(&statements);
    free(sql_text);
    return had_error ? 1 : 0;
}

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "ERROR: invalid query\n");
        return 1;
    }

    return process_sql_file(argv[1]);
}
