#include "executor.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* SQL 문장에서 언급한 테이블의 스키마를 찾습니다. */
static const TableDef *find_table_def(const TableDef *tables, int table_count, const char *name) {
    int i;

    for (i = 0; i < table_count; i++) {
        if (strcmp(tables[i].name, name) == 0) {
            return &tables[i];
        }
    }

    return NULL;
}

/* 내부 에러 코드를 과제에서 요구한 정확한 메시지로 바꿔 출력합니다. */
static int print_error(ErrorCode code) {
    switch (code) {
        case ERR_TABLE_NOT_FOUND:
            fprintf(stderr, "ERROR: table not found\n");
            return -1;
        case ERR_COLUMN_MISMATCH:
            fprintf(stderr, "ERROR: column count does not match value count\n");
            return -1;
        case ERR_INVALID_QUERY:
            fprintf(stderr, "ERROR: invalid query\n");
            return -1;
        case ERR_FILE_OPEN:
            fprintf(stderr, "ERROR: file open failed\n");
            return -1;
        case ERR_NONE:
        default:
            return 0;
    }
}

/* INSERT의 컬럼 목록이 스키마와 맞는지 검사합니다.
   컬럼 순서가 다르거나 빠진 경우를 여기서 걸러냅니다. */
static ErrorCode validate_insert_shape(const Command *cmd, const TableDef *table) {
    int i;

    if (cmd->insert_column_count != table->column_count) {
        return ERR_COLUMN_MISMATCH;
    }

    for (i = 0; i < table->column_count; i++) {
        if (strcmp(cmd->insert_columns[i], table->columns[i].name) != 0) {
            return ERR_INVALID_QUERY;
        }
    }

    return ERR_NONE;
}

/* ResultSet을 테스트가 기대하는 출력 형식으로 화면에 찍습니다. */
static void print_result_set(const ResultSet *result) {
    int row;
    int col;

    if (result->row_count == 0) {
        return;
    }

    for (col = 0; col < result->header_count; col++) {
        if (col > 0) {
            putchar(',');
        }
        fputs(result->headers[col], stdout);
    }
    putchar('\n');

    for (row = 0; row < result->row_count; row++) {
        for (col = 0; col < result->header_count; col++) {
            if (col > 0) {
                putchar(',');
            }
            fputs(result->rows[row][col], stdout);
        }
        putchar('\n');
    }
}

/* 파싱이 끝난 SQL 명령 1개를 실제로 실행합니다.
   parser가 문장 구조를 정리해 두었고,
   여기서는 그 구조를 보고 어떤 행동을 할지 결정합니다. */
int execute_command(const Command *cmd, StorageOps *ops, const TableDef *tables, int table_count) {
    if (cmd->type == CMD_INSERT) {
        const TableDef *table = find_table_def(tables, table_count, cmd->table_name);
        ErrorCode status;

        /* 스키마에 없는 테이블로 INSERT하면 바로 에러입니다. */
        if (table == NULL) {
            return print_error(ERR_TABLE_NOT_FOUND);
        }

        /* 파일에 쓰기 전에 INSERT 모양이 스키마와 맞는지 먼저 확인합니다. */
        status = validate_insert_shape(cmd, table);
        if (status != ERR_NONE) {
            return print_error(status);
        }

        /* 실제 파일 저장은 저장소 백엔드에 맡깁니다. */
        status = (ErrorCode)ops->insert(ops->ctx, cmd->table_name, cmd->values, cmd->value_count);
        return print_error(status);
    }

    if (cmd->type == CMD_SELECT) {
        ResultSet *result = (ResultSet *)malloc(sizeof(*result));
        ErrorCode status;

        /* 결과 버퍼가 커서 스택에 두면 터질 수 있으므로
           힙 메모리를 사용합니다. */
        if (result == NULL) {
            return print_error(ERR_INVALID_QUERY);
        }

        memset(result, 0, sizeof(*result));
        /* 저장소 백엔드에게 데이터를 읽어 ResultSet을 채워 달라고 요청합니다. */
        status = (ErrorCode)ops->select_rows(ops->ctx,
                                             cmd->table_name,
                                             cmd->columns,
                                             cmd->column_count,
                                             cmd->is_select_all,
                                             result);
        if (status != ERR_NONE) {
            free(result);
            return print_error(status);
        }

        /* SELECT 결과를 출력한 뒤 임시 버퍼를 해제합니다. */
        print_result_set(result);
        free(result);
        return 0;
    }

    return 0;
}
