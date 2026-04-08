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

/* 파일에서 SQL 문장 1개만 읽어옵니다.
   문장은 세미콜론(;)에서 끝나지만,
   작은따옴표 안에 있는 세미콜론은 데이터이므로 문장 끝으로 보면 안 됩니다. */
static int read_next_statement(FILE *fp, char *buffer, size_t buffer_size) {
    int ch;
    int in_quote = 0;
    int overflow = 0;
    size_t len = 0;

    while ((ch = fgetc(fp)) != EOF) {
        /* 버퍼가 꽉 차기 전까지는 문자를 계속 저장합니다.
           버퍼가 꽉 찬 뒤에도 문장 끝을 찾기 위해 파일은 계속 읽습니다. */
        if (!overflow) {
            if (len + 1 < buffer_size) {
                buffer[len++] = (char)ch;
            } else {
                overflow = 1;
            }
        }

        /* 작은따옴표를 만나면 지금이 문자열 안인지 밖인지 상태를 바꿉니다. */
        if (ch == '\'') {
            in_quote = !in_quote;
        }

        /* 세미콜론은 작은따옴표 밖에 있을 때만 문장의 끝입니다. */
        if (ch == ';' && !in_quote) {
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
    if (ops == NULL || ops->init == NULL || ops->insert == NULL ||
        ops->select_rows == NULL || ops->destroy == NULL ||
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
