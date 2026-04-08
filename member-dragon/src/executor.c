#include "executor.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "storage.h"

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

/* 테이블 안에서 특정 컬럼이 몇 번째인지 찾습니다.
   없으면 -1을 돌려줘서 잘못된 컬럼 이름을 잡아냅니다. */
static int find_column_index_in_table(const TableDef *table, const char *column_name) {
    int i;

    for (i = 0; i < table->column_count; i++) {
        if (strcmp(table->columns[i].name, column_name) == 0) {
            return i;
        }
    }

    return -1;
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

/* 두 값을 비교합니다.
   둘 다 정수처럼 보이면 숫자로 비교하고,
   아니면 글자 순서(strcmp)로 비교합니다. */
static int compare_values(const char *left, const char *right) {
    char *left_end;
    char *right_end;
    long left_number = strtol(left, &left_end, 10);
    long right_number = strtol(right, &right_end, 10);

    /* 문자열 끝까지 모두 숫자로 읽혔을 때만
       "진짜 정수 값"으로 보고 숫자 비교를 합니다. */
    if (*left_end == '\0' && *right_end == '\0') {
        if (left_number < right_number) {
            return -1;
        }
        if (left_number > right_number) {
            return 1;
        }
        return 0;
    }

    return strcmp(left, right);
}

/* 비교 결과와 WHERE 연산자를 묶어서 최종 참/거짓을 판단합니다. */
static int compare_with_operator(int cmp_result, CompareOp op) {
    switch (op) {
        case OP_EQ:
            return cmp_result == 0;
        case OP_NE:
            return cmp_result != 0;
        case OP_GT:
            return cmp_result > 0;
        case OP_LT:
            return cmp_result < 0;
        case OP_GE:
            return cmp_result >= 0;
        case OP_LE:
            return cmp_result <= 0;
        default:
            return 0;
    }
}

/* 현재 row가 WHERE 조건에 맞는지 검사합니다. */
static int row_matches_where(const ResultSet *result, int row_index,
                             int column_index, const WhereCondition *where) {
    int cmp_result = compare_values(result->rows[row_index][column_index], where->value);

    /* compare_values는 "크다/같다/작다" 결과만 주므로,
       그 결과를 WHERE의 실제 연산자 의미로 다시 해석합니다. */
    return compare_with_operator(cmp_result, where->op);
}

/* row 복사는 여러 기능에서 반복되므로 따로 묶어 둡니다. */
static void copy_row_values(char dest[][MAX_VALUE_LEN],
                            const char src[][MAX_VALUE_LEN], int column_count) {
    int col;

    for (col = 0; col < column_count; col++) {
        strcpy(dest[col], src[col]);
    }
}

/* INSERT 컬럼 순서를 스키마 순서로 다시 맞춥니다.
   예: (major, name, age)로 들어와도 (name, age, major) 순서로 바꿉니다. */
static ErrorCode reorder_insert_values(const Command *cmd, const TableDef *table,
                                       char reordered[][MAX_VALUE_LEN]) {
    int seen[MAX_COLUMNS] = {0};
    int i;

    /* 값 개수, 입력 컬럼 개수, 스키마 컬럼 개수가 다르면
       어느 칸에 무엇을 넣어야 할지 자체가 성립하지 않습니다. */
    if (cmd->insert_column_count != cmd->value_count ||
        cmd->insert_column_count != table->column_count) {
        return ERR_COLUMN_MISMATCH;
    }

    for (i = 0; i < cmd->insert_column_count; i++) {
        int schema_index = find_column_index_in_table(table, cmd->insert_columns[i]);

        /* 스키마에 없는 컬럼 이름이면 잘못된 INSERT입니다. */
        if (schema_index < 0) {
            return ERR_INVALID_QUERY;
        }

        /* 같은 컬럼이 두 번 들어오면 어느 값을 써야 할지 모호하므로 막습니다. */
        if (seen[schema_index]) {
            return ERR_INVALID_QUERY;
        }

        /* 입력 순서가 어떻든 스키마가 기대하는 자리에 값을 옮겨 담습니다. */
        strcpy(reordered[schema_index], cmd->values[i]);
        seen[schema_index] = 1;
    }

    /* schema 전체 컬럼이 정확히 한 번씩 채워졌는지 마지막으로 확인합니다. */
    for (i = 0; i < table->column_count; i++) {
        if (!seen[i]) {
            return ERR_INVALID_QUERY;
        }
    }

    return ERR_NONE;
}

/* WHERE 컬럼이 실제 스키마에 있는지 확인하고 위치를 돌려줍니다. */
static ErrorCode resolve_where_column(const Command *cmd, const TableDef *table, int *column_index) {
    if (!cmd->has_where) {
        /* WHERE가 없으면 이후 로직이 이 값을 쓰지 않으므로 -1로만 표시합니다. */
        *column_index = -1;
        return ERR_NONE;
    }

    *column_index = find_column_index_in_table(table, cmd->where.column);
    return *column_index >= 0 ? ERR_NONE : ERR_INVALID_QUERY;
}

/* ORDER BY 컬럼이 실제 스키마에 있는지 확인하고 위치를 돌려줍니다. */
static ErrorCode resolve_order_by_column(const Command *cmd, const TableDef *table, int *column_index) {
    if (!cmd->has_order_by) {
        /* ORDER BY가 없는 SELECT는 원래 읽힌 순서 그대로 출력하면 됩니다. */
        *column_index = -1;
        return ERR_NONE;
    }

    *column_index = find_column_index_in_table(table, cmd->order_by.column);
    return *column_index >= 0 ? ERR_NONE : ERR_INVALID_QUERY;
}

/* UPDATE의 SET 대상 컬럼들을 미리 확인합니다.
   없는 컬럼이나 중복 컬럼이 있으면 실행 전에 막습니다. */
static ErrorCode resolve_set_columns(const Command *cmd, const TableDef *table, int indices[]) {
    int seen[MAX_COLUMNS] = {0};
    int i;

    for (i = 0; i < cmd->set_clause_count; i++) {
        /* SET major = 'x'에서 major가 실제 컬럼인지 먼저 찾습니다. */
        indices[i] = find_column_index_in_table(table, cmd->set_clauses[i].column);
        if (indices[i] < 0) {
            return ERR_INVALID_QUERY;
        }
        /* 같은 컬럼을 SET에서 두 번 바꾸면 의미가 애매하므로 막습니다. */
        if (seen[indices[i]]) {
            return ERR_INVALID_QUERY;
        }
        seen[indices[i]] = 1;
    }

    return ERR_NONE;
}

/* WHERE 조건에 맞는 row만 앞쪽으로 당겨서 남깁니다. */
static void filter_rows_in_place(ResultSet *result, int column_index, const WhereCondition *where) {
    int read_row;
    int write_row = 0;

    for (read_row = 0; read_row < result->row_count; read_row++) {
        if (row_matches_where(result, read_row, column_index, where)) {
            /* 남길 row만 앞에서부터 다시 채우면
               별도 메모리 없이도 제자리 필터링이 가능합니다. */
            if (write_row != read_row) {
                copy_row_values(result->rows[write_row], result->rows[read_row], result->header_count);
            }
            write_row++;
        }
    }

    result->row_count = write_row;
}

/* DELETE는 WHERE에 맞지 않는 row만 남기면 됩니다. */
static void delete_rows_in_place(ResultSet *result, int column_index, const WhereCondition *where) {
    int read_row;
    int write_row = 0;

    for (read_row = 0; read_row < result->row_count; read_row++) {
        if (!row_matches_where(result, read_row, column_index, where)) {
            /* 삭제 대상이 아닌 row만 앞으로 당겨 남깁니다. */
            if (write_row != read_row) {
                copy_row_values(result->rows[write_row], result->rows[read_row], result->header_count);
            }
            write_row++;
        }
    }

    result->row_count = write_row;
}

/* ORDER BY가 있으면 row 순서를 바꿉니다.
   데이터 양이 작으므로 이해하기 쉬운 버블 정렬로 구현합니다. */
static void sort_rows_in_place(ResultSet *result, int column_index, OrderDirection direction) {
    int pass;
    int row;

    for (pass = 0; pass < result->row_count - 1; pass++) {
        for (row = 0; row < result->row_count - 1 - pass; row++) {
            int cmp_result = compare_values(result->rows[row][column_index],
                                            result->rows[row + 1][column_index]);
            int should_swap = (direction == ORDER_ASC) ? (cmp_result > 0) : (cmp_result < 0);

            /* 정렬 방향에 맞지 않는 두 row를 만나면 자리를 바꿉니다. */
            if (should_swap) {
                char temp_row[MAX_COLUMNS][MAX_VALUE_LEN];

                copy_row_values(temp_row, result->rows[row], result->header_count);
                copy_row_values(result->rows[row], result->rows[row + 1], result->header_count);
                copy_row_values(result->rows[row + 1], temp_row, result->header_count);
            }
        }
    }
}

/* LIMIT은 남길 row 개수만 줄이면 됩니다. */
static void apply_limit_in_place(ResultSet *result, int limit_count) {
    /* 이미 row가 limit 이하라면 건드릴 필요가 없습니다. */
    if (limit_count < result->row_count) {
        result->row_count = limit_count;
    }
}

/* SELECT 컬럼 목록에 맞춰 결과를 마지막에 투영합니다.
   WHERE/ORDER BY는 전체 row 기준으로 끝낸 뒤에 여기서 필요한 컬럼만 남깁니다. */
static ErrorCode project_result_set(ResultSet *result, const TableDef *table, const Command *cmd) {
    int selected_indices[MAX_COLUMNS];
    int col;
    int row;

    /* SELECT * 는 full-row 결과를 그대로 출력하면 되므로
       헤더만 스키마 이름으로 다시 맞춰 주면 됩니다. */
    if (cmd->is_select_all) {
        result->header_count = table->column_count;
        for (col = 0; col < table->column_count; col++) {
            strcpy(result->headers[col], table->columns[col].name);
        }
        return ERR_NONE;
    }

    /* 요청한 컬럼 이름을 실제 스키마 인덱스로 바꿔 둡니다. */
    for (col = 0; col < cmd->column_count; col++) {
        selected_indices[col] = find_column_index_in_table(table, cmd->columns[col]);
        if (selected_indices[col] < 0) {
            return ERR_INVALID_QUERY;
        }
    }

    for (row = 0; row < result->row_count; row++) {
        char projected_row[MAX_COLUMNS][MAX_VALUE_LEN];

        /* 한 row에서 필요한 값만 임시로 모은 뒤, */
        for (col = 0; col < cmd->column_count; col++) {
            strcpy(projected_row[col], result->rows[row][selected_indices[col]]);
        }

        /* row 앞부분을 projection 결과로 덮어씁니다. */
        for (col = 0; col < cmd->column_count; col++) {
            strcpy(result->rows[row][col], projected_row[col]);
        }
    }

    for (col = 0; col < cmd->column_count; col++) {
        strcpy(result->headers[col], cmd->columns[col]);
    }
    result->header_count = cmd->column_count;
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

/* SELECT는 전체 row를 읽은 뒤 WHERE/ORDER BY/LIMIT을 적용하고,
   마지막에 필요한 컬럼만 골라서 출력합니다. */
static int execute_select(const Command *cmd, StorageOps *ops, const TableDef *table) {
    ResultSet *result = (ResultSet *)malloc(sizeof(*result));
    ErrorCode status;
    int where_column_index;
    int order_by_column_index;

    if (result == NULL) {
        return print_error(ERR_INVALID_QUERY);
    }

    result_set_reset(result);

    /* Phase 2의 핵심:
       먼저 테이블 전체 row를 읽어 와야 WHERE와 ORDER BY를
       출력 컬럼과 무관하게 안전하게 적용할 수 있습니다. */
    status = (ErrorCode)ops->read_all_rows(ops->ctx, cmd->table_name, result);
    if (status != ERR_NONE) {
        free(result);
        return print_error(status);
    }

    status = resolve_where_column(cmd, table, &where_column_index);
    if (status != ERR_NONE) {
        free(result);
        return print_error(status);
    }

    status = resolve_order_by_column(cmd, table, &order_by_column_index);
    if (status != ERR_NONE) {
        free(result);
        return print_error(status);
    }

    /* 실행 순서는 SQL 의미에 맞게
       WHERE -> ORDER BY -> LIMIT -> projection 입니다. */
    if (cmd->has_where) {
        filter_rows_in_place(result, where_column_index, &cmd->where);
    }

    if (cmd->has_order_by) {
        sort_rows_in_place(result, order_by_column_index, cmd->order_by.direction);
    }

    if (cmd->has_limit) {
        apply_limit_in_place(result, cmd->limit_count);
    }

    status = project_result_set(result, table, cmd);
    if (status != ERR_NONE) {
        free(result);
        return print_error(status);
    }

    /* 여기까지 오면 result는 "실제로 화면에 보여 줄 최종 결과"가 됩니다. */
    print_result_set(result);
    free(result);
    return 0;
}

/* DELETE는 남겨둘 row만 추린 뒤 파일 전체를 다시 씁니다. */
static int execute_delete(const Command *cmd, StorageOps *ops, const TableDef *table) {
    ResultSet *result = (ResultSet *)malloc(sizeof(*result));
    ErrorCode status;
    int where_column_index;

    if (result == NULL) {
        return print_error(ERR_INVALID_QUERY);
    }

    result_set_reset(result);

    status = (ErrorCode)ops->read_all_rows(ops->ctx, cmd->table_name, result);
    if (status != ERR_NONE) {
        free(result);
        return print_error(status);
    }

    status = resolve_where_column(cmd, table, &where_column_index);
    if (status != ERR_NONE) {
        free(result);
        return print_error(status);
    }

    if (cmd->has_where) {
        /* WHERE가 있으면 조건에 맞는 row만 지웁니다. */
        delete_rows_in_place(result, where_column_index, &cmd->where);
    } else {
        /* WHERE가 없으면 테이블 전체를 비우는 DELETE입니다. */
        result->row_count = 0;
    }

    /* 바뀐 전체 테이블 상태를 파일에 다시 저장합니다. */
    status = (ErrorCode)ops->replace_rows(ops->ctx, cmd->table_name, result);
    free(result);
    return print_error(status);
}

/* UPDATE는 조건에 맞는 row만 바꾼 뒤 전체 파일을 다시 씁니다. */
static int execute_update(const Command *cmd, StorageOps *ops, const TableDef *table) {
    ResultSet *result = (ResultSet *)malloc(sizeof(*result));
    ErrorCode status;
    int where_column_index;
    int set_indices[MAX_SET_CLAUSES];
    int row;
    int set_index;

    if (result == NULL) {
        return print_error(ERR_INVALID_QUERY);
    }

    result_set_reset(result);

    status = (ErrorCode)ops->read_all_rows(ops->ctx, cmd->table_name, result);
    if (status != ERR_NONE) {
        free(result);
        return print_error(status);
    }

    status = resolve_where_column(cmd, table, &where_column_index);
    if (status != ERR_NONE) {
        free(result);
        return print_error(status);
    }

    status = resolve_set_columns(cmd, table, set_indices);
    if (status != ERR_NONE) {
        free(result);
        return print_error(status);
    }

    for (row = 0; row < result->row_count; row++) {
        /* WHERE가 없으면 모든 row,
           WHERE가 있으면 조건에 맞는 row만 값을 바꿉니다. */
        if (!cmd->has_where || row_matches_where(result, row, where_column_index, &cmd->where)) {
            for (set_index = 0; set_index < cmd->set_clause_count; set_index++) {
                /* 미리 찾아 둔 schema 인덱스 위치에 새 값을 덮어씁니다. */
                strcpy(result->rows[row][set_indices[set_index]], cmd->set_clauses[set_index].value);
            }
        }
    }

    /* 수정된 전체 테이블을 파일에 다시 써서 UPDATE 결과를 확정합니다. */
    status = (ErrorCode)ops->replace_rows(ops->ctx, cmd->table_name, result);
    free(result);
    return print_error(status);
}

/* 파싱이 끝난 SQL 명령 1개를 실제로 실행합니다.
   parser가 문장 구조를 정리해 두었고,
   여기서는 그 구조를 보고 어떤 행동을 할지 결정합니다. */
int execute_command(const Command *cmd, StorageOps *ops, const TableDef *tables, int table_count) {
    const TableDef *table;

    /* 빈 줄은 parse_sql 단계에서 CMD_NONE으로 오므로 아무 일도 하지 않습니다. */
    if (cmd->type == CMD_NONE) {
        return 0;
    }

    /* 대부분의 명령은 먼저 테이블 정의를 알아야 하므로 한 번만 찾아 둡니다. */
    table = find_table_def(tables, table_count, cmd->table_name);

    if (cmd->type == CMD_INSERT) {
        char reordered_values[MAX_VALUES][MAX_VALUE_LEN];
        ErrorCode status;

        /* 스키마에 없는 테이블로 INSERT하면 바로 에러입니다. */
        if (table == NULL) {
            return print_error(ERR_TABLE_NOT_FOUND);
        }

        /* 컬럼 순서가 달라도 스키마 순서로 다시 맞춰 저장합니다.
           이 단계에서 중복 컬럼, 누락 컬럼, 없는 컬럼도 함께 걸러집니다. */
        status = reorder_insert_values(cmd, table, reordered_values);
        if (status != ERR_NONE) {
            return print_error(status);
        }

        /* 실제 파일 저장은 저장소 백엔드에 맡깁니다. */
        status = (ErrorCode)ops->insert(ops->ctx, cmd->table_name,
                                        reordered_values, table->column_count);
        return print_error(status);
    }

    if (cmd->type == CMD_SELECT) {
        if (table == NULL) {
            return print_error(ERR_TABLE_NOT_FOUND);
        }
        return execute_select(cmd, ops, table);
    }

    if (cmd->type == CMD_DELETE) {
        if (table == NULL) {
            return print_error(ERR_TABLE_NOT_FOUND);
        }
        return execute_delete(cmd, ops, table);
    }

    if (cmd->type == CMD_UPDATE) {
        if (table == NULL) {
            return print_error(ERR_TABLE_NOT_FOUND);
        }
        return execute_update(cmd, ops, table);
    }

    return 0;
}
