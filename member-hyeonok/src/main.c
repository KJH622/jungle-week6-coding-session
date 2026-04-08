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
        ParseResult parse_result;

        /*
         * 빈 문장은 splitter 단계에서 제거되므로,
         * 여기서는 실제 SQL 문장만 순차적으로 parser / executor 에 넘긴다.
         */
        parse_result = parse_query(statements.items[index], &statement);
        if (parse_result != PARSE_SUCCESS) {
            /*
             * 문법 오류와 내부 오류를 호출부에서 구분할 수 있게 해 두고,
             * 현재 단계에서는 팀 공통 규칙에 맞는 표준 에러 문구로 연결한다.
             */
            if (parse_result == PARSE_INVALID_QUERY) {
                fprintf(stderr, "ERROR: invalid query\n");
            } else {
                fprintf(stderr, "ERROR: file open failed\n");
            }
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
