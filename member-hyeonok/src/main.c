#include "executor.h"
#include "parser.h"
#include "sql_splitter.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>

static void print_parse_error(ParseResult parse_result)
{
    /*
     * 파서는 표준 출력에 관여하지 않고 결과 코드만 돌려준다.
     * 사용자에게 보여줄 에러 문구 선택은 main 이 한곳에서 담당한다.
     */
    if (parse_result == PARSE_INVALID_QUERY) {
        fprintf(stderr, "ERROR: invalid query\n");
    } else {
        fprintf(stderr, "ERROR: file open failed\n");
    }
}

static void print_execute_error(ExecuteResult execute_result)
{
    /*
     * executor 역시 stdout 결과만 책임지고, stderr 문구 결정은 호출부에서 맡는다.
     * 이렇게 분리해 두면 다음 단계에서 에러 정책을 바꿔도 executor 내부를 건드릴 일이 줄어든다.
     */
    if (execute_result == EXECUTE_INVALID_QUERY) {
        fprintf(stderr, "ERROR: invalid query\n");
    } else if (execute_result == EXECUTE_TABLE_NOT_FOUND) {
        fprintf(stderr, "ERROR: table not found\n");
    } else if (execute_result == EXECUTE_COLUMN_COUNT_MISMATCH) {
        fprintf(stderr, "ERROR: column count does not match value count\n");
    } else {
        fprintf(stderr, "ERROR: file open failed\n");
    }
}

static int run_single_statement(const char *sql_text)
{
    SqlStatement statement;
    ParseResult parse_result;
    ExecuteResult execute_result;

    /*
     * 각 SQL 문장은 독립적으로 파싱/실행한다.
     * 한 문장에서 실패하더라도 호출한 쪽이 다음 문장으로 계속 진행할 수 있게
     * 성공이면 0, 실패면 1만 반환한다.
     */
    parse_result = parse_query(sql_text, &statement);
    if (parse_result != PARSE_SUCCESS) {
        print_parse_error(parse_result);
        return 1;
    }

    execute_result = execute_query(&statement);
    if (execute_result != EXECUTE_SUCCESS) {
        print_execute_error(execute_result);
        free_sql_statement(&statement);
        return 1;
    }

    free_sql_statement(&statement);
    return 0;
}

static int process_sql_file(const char *sql_path)
{
    char *sql_text;
    SqlStatementList statements;
    size_t index;
    int had_failure;

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

    /*
     * 전체 종료코드는 "하나라도 실패했는가"만 보면 되므로,
     * 문장별 결과를 누적해서 마지막에 한 번만 반환한다.
     */
    had_failure = 0;

    for (index = 0; index < statements.count; index++) {
        /*
         * 빈 문장은 splitter 단계에서 제거되므로,
         * 여기서는 실제 SQL 문장만 순차적으로 parser / executor 에 넘긴다.
         */
        if (run_single_statement(statements.items[index]) != 0) {
            had_failure = 1;
        }
    }

    free_sql_statement_list(&statements);
    free(sql_text);
    return had_failure ? 1 : 0;
}

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "ERROR: invalid query\n");
        return 1;
    }

    return process_sql_file(argv[1]);
}
