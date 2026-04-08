#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "executor.h"
#include "file_storage.h"
#include "parser.h"
#include "schema.h"

/* 문자열 안에 공백이 아닌 문자가 하나라도 있으면 1을 반환합니다.
   파일 끝부분에 남은 빈 줄이나 공백만 있는 부분을 무시할 때 씁니다. */
static int has_non_whitespace(const char *text) {
    while (*text != '\0') {
        if (!isspace((unsigned char)*text)) {
            return 1;
        }
        text++;
    }
    return 0;
}

/* SQL 문장을 읽는 동안 문자 1개를 버퍼에 붙입니다.
   버퍼가 꽉 찬 뒤에도 문장 끝은 찾아야 하므로,
   overflow가 나면 저장만 멈추고 읽기는 계속 진행합니다. */
static void append_statement_char(char *buffer, size_t buffer_size,
                                  size_t *len, int *overflow, int ch) {
    if (*overflow) {
        return;
    }

    if (*len + 1 < buffer_size) {
        buffer[*len] = (char)ch;
        (*len)++;
        return;
    }

    *overflow = 1;
}

/* 파일에서 SQL 문장 1개만 읽어옵니다.
   문장은 세미콜론(;)에서 끝나지만,
   작은따옴표 안에 있는 세미콜론은 데이터이므로 문장 끝으로 보면 안 됩니다.
   또한 SQL 문자열 안의 escaped quote(`''`)는 문자열 종료가 아니라
   값 안의 작은따옴표 1개로 취급해야 합니다. */
static int read_next_statement(FILE *fp, char *buffer, size_t buffer_size) {
    int ch;
    int in_quote = 0;
    int overflow = 0;
    int pushed_ch = EOF;
    size_t len = 0;

    for (;;) {
        if (pushed_ch != EOF) {
            ch = pushed_ch;
            pushed_ch = EOF;
        } else {
            ch = fgetc(fp);
        }

        if (ch == EOF) {
            break;
        }

        append_statement_char(buffer, buffer_size, &len, &overflow, ch);

        /* 작은따옴표를 만나면 문자열 시작/종료를 판단합니다.
           단, 문자열 안에서 ''가 나오면 escaped quote이므로
           아직 문자열이 끝난 것이 아닙니다. */
        if (ch == '\'') {
            if (!in_quote) {
                in_quote = 1;
                continue;
            }

            ch = fgetc(fp);
            if (ch == '\'') {
                append_statement_char(buffer, buffer_size, &len, &overflow, ch);
                continue;
            }

            in_quote = 0;
            pushed_ch = ch;
            continue;
        }

        /* 세미콜론은 작은따옴표 밖에 있을 때만 문장의 끝입니다. */
        if (!in_quote && ch == ';') {
            break;
        }
    }

    buffer[len] = '\0';

    /* 파일 끝까지 읽었는데 공백만 모였다면 처리할 다음 문장이 없는 것입니다. */
    if (ch == EOF && !has_non_whitespace(buffer)) {
        return 0;
    }

    return 1;
}

/* 프로그램 전체 흐름:
   1. 입력 .sql 파일 열기
   2. 스키마 읽기
   3. 파일 기반 저장소 만들기
   4. SQL 문장 1개씩 읽기
   5. 파싱 후 실행 */
int main(int argc, char *argv[]) {
    FILE *fp;
    TableDef tables[MAX_TABLES];
    int table_count;
    StorageOps *ops;
    int has_error = 0;
    char statement[8192];

    if (argc < 2) {
        fprintf(stderr, "ERROR: file open failed\n");
        return 1;
    }

    fp = fopen(argv[1], "r");
    if (fp == NULL) {
        fprintf(stderr, "ERROR: file open failed\n");
        return 1;
    }

    /* 스키마 파일에는 어떤 테이블이 있고 컬럼이 무엇인지 들어 있습니다. */
    table_count = load_schemas(".", tables, MAX_TABLES);
    if (table_count < 0) {
        fclose(fp);
        fprintf(stderr, "ERROR: file open failed\n");
        return 1;
    }

    /* 저장소 백엔드를 만들고 스키마 정보를 넘깁니다.
       이번 프로젝트에서는 <table>.data 파일을 읽고 쓰는 저장소를 사용합니다. */
    ops = file_storage_create();
    /* Phase 2부터는 read_all_rows / replace_rows도 꼭 있어야
       WHERE, ORDER BY, DELETE, UPDATE가 정상 동작할 수 있습니다. */
    if (ops == NULL || ops->init == NULL || ops->insert == NULL ||
        ops->select_rows == NULL || ops->read_all_rows == NULL ||
        ops->replace_rows == NULL || ops->destroy == NULL ||
        ops->init(ops->ctx, tables, table_count) != ERR_NONE) {
        if (ops != NULL) {
            if (ops->destroy != NULL && ops->ctx != NULL) {
                ops->destroy(ops->ctx);
            }
            free(ops);
        }
        fclose(fp);
        fprintf(stderr, "ERROR: file open failed\n");
        return 1;
    }

    /* 입력 파일을 SQL 문장 단위로 처리합니다.
       중간에 에러가 나도 다음 문장으로 계속 진행해야 합니다. */
    while (read_next_statement(fp, statement, sizeof(statement))) {
        Command cmd;

        if (parse_sql(statement, &cmd) != 0) {
            fprintf(stderr, "ERROR: invalid query\n");
            has_error = 1;
            continue;
        }

        if (cmd.type == CMD_NONE) {
            continue;
        }

        if (execute_command(&cmd, ops, tables, table_count) != 0) {
            has_error = 1;
        }
    }

    /* 프로그램이 끝나기 전에 열어 둔 자원을 정리합니다. */
    fclose(fp);
    ops->destroy(ops->ctx);
    free(ops);

    /* SQL 문장 중 하나라도 실패했으면 종료 코드를 1로 돌려줍니다. */
    return has_error ? 1 : 0;
}
