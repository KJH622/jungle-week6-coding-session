#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "include/ast.h"
#include "include/executor.h"
#include "include/parser.h"
#include "include/status.h"

/* SQL 입력 파일 전체를 읽어 메모리 버퍼로 반환한다. */
static char *read_sql_file(const char *path, SqlStatus *status)
{
    FILE *file;
    long file_size;
    size_t read_count;
    char *buffer;

    file = fopen(path, "rb");
    if (file == NULL) {
        *status = SQL_STATUS_FILE_OPEN_FAILED;
        return NULL;
    }

    if (fseek(file, 0L, SEEK_END) != 0) {
        fclose(file);
        *status = SQL_STATUS_FILE_OPEN_FAILED;
        return NULL;
    }

    file_size = ftell(file);
    if (file_size < 0L) {
        fclose(file);
        *status = SQL_STATUS_FILE_OPEN_FAILED;
        return NULL;
    }

    if (fseek(file, 0L, SEEK_SET) != 0) {
        fclose(file);
        *status = SQL_STATUS_FILE_OPEN_FAILED;
        return NULL;
    }

    buffer = (char *)malloc((size_t)file_size + 1U);
    if (buffer == NULL) {
        fclose(file);
        *status = SQL_STATUS_INTERNAL_ERROR;
        return NULL;
    }

    read_count = fread(buffer, 1U, (size_t)file_size, file);
    fclose(file);

    if (read_count != (size_t)file_size) {
        free(buffer);
        *status = SQL_STATUS_FILE_OPEN_FAILED;
        return NULL;
    }

    buffer[file_size] = '\0';
    *status = SQL_STATUS_OK;
    return buffer;
}

/* 상태 코드를 과제 형식의 에러 메시지로 출력한다. */
static void print_error(FILE *error_stream, SqlStatus status)
{
    fprintf(error_stream, "ERROR: %s\n", sql_status_message(status));
}

/* 프로그램 진입점: 파일 읽기, 파싱, 실행, flush까지 전체 흐름을 처리한다. */
int main(int argc, char **argv)
{
    char *sql_text;
    char **statements;
    size_t statement_count;
    size_t index;
    bool had_error;
    ExecutionContext context;
    SqlStatus status;

    if (argc != 2) {
        print_error(stderr, SQL_STATUS_FILE_OPEN_FAILED);
        return 1;
    }

    sql_text = read_sql_file(argv[1], &status);
    if (sql_text == NULL) {
        print_error(stderr, status);
        return 1;
    }

    status = split_sql_statements(sql_text, &statements, &statement_count);
    if (status != SQL_STATUS_OK) {
        free(sql_text);
        print_error(stderr, status);
        return 1;
    }

    status = execution_context_init(&context, ".", ".");
    if (status != SQL_STATUS_OK) {
        free_statement_list(statements, statement_count);
        free(sql_text);
        print_error(stderr, status);
        return 1;
    }

    had_error = false;
    for (index = 0U; index < statement_count; index++) {
        Statement statement;

        status = parse_statement(statements[index], &statement);
        if (status != SQL_STATUS_OK) {
            print_error(stderr, status);
            had_error = true;
            continue;
        }

        execute_statement(&context, &statement, stdout, stderr, &had_error);
        statement_destroy(&statement);
    }

    status = execution_context_flush(&context);
    if (status != SQL_STATUS_OK) {
        print_error(stderr, status);
        had_error = true;
    }

    execution_context_destroy(&context);
    free_statement_list(statements, statement_count);
    free(sql_text);

    return had_error ? 1 : 0;
}
