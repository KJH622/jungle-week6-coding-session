#include "file_storage.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "storage.h"

#define MAX_PATH_LEN 1024
#define MAX_ROW_LINE_LEN 16384

/* 파일 기반 저장소가 실행 중에 들고 있어야 하는 정보입니다. */
typedef struct {
    TableDef tables[MAX_TABLES];
    int table_count;
    char data_dir[MAX_PATH_LEN];
} FileStore;

/* 테이블 이름에 맞는 스키마 정보를 찾습니다. */
static const TableDef *find_table(const FileStore *store, const char *name) {
    int i;

    for (i = 0; i < store->table_count; i++) {
        if (strcmp(store->tables[i].name, name) == 0) {
            return &store->tables[i];
        }
    }

    return NULL;
}

/* 컬럼 이름이 몇 번째 컬럼인지 찾습니다. */
static int find_column_index(const TableDef *table, const char *name) {
    int i;

    for (i = 0; i < table->column_count; i++) {
        if (strcmp(table->columns[i].name, name) == 0) {
            return i;
        }
    }

    return -1;
}

/* 테이블의 실제 데이터 파일 경로를 만듭니다.
   예: /some/path/users.data */
static int build_table_path(const FileStore *store, const char *table_name,
                            char *path, size_t path_size) {
    int written = snprintf(path, path_size, "%s/%s.data", store->data_dir, table_name);

    return written > 0 && (size_t)written < path_size ? 0 : -1;
}

/* ResultSet 헤더를 스키마 순서대로 채웁니다.
   파일에서 읽은 row도 이 순서를 기준으로 저장합니다.
   즉 row[0]은 항상 첫 번째 컬럼, row[1]은 두 번째 컬럼이라는 약속을 만드는 함수입니다. */
static void set_full_headers(const TableDef *table, ResultSet *out) {
    int col;

    result_set_reset(out);
    out->header_count = table->column_count;

    for (col = 0; col < table->column_count; col++) {
        strcpy(out->headers[col], table->columns[col].name);
    }
}

/* 행 1개를 길이+값 형식으로 저장합니다.
   예:
   9:김민준2:2515:컴퓨터공학
   값 안에 쉼표가 들어가도 CSV보다 안전하게 구분할 수 있습니다. */
static int write_encoded_row(FILE *fp, const char values[][MAX_VALUE_LEN], int value_count) {
    int i;

    for (i = 0; i < value_count; i++) {
        size_t len = strlen(values[i]);

        /* 먼저 값 길이, 그다음 콜론(:), 그다음 실제 값을 저장합니다. */
        if (fprintf(fp, "%zu:", len) < 0) {
            return -1;
        }
        if (len > 0 && fwrite(values[i], 1, len, fp) != len) {
            return -1;
        }
    }

    return fputc('\n', fp) == EOF ? -1 : 0;
}

/* 콜론(:) 앞에 있는 숫자를 읽습니다.
   예: "9:김민준"이면 9를 읽습니다. */
static int parse_next_length(const char **cursor, size_t *out_len) {
    size_t len = 0;
    int saw_digit = 0;

    while (**cursor >= '0' && **cursor <= '9') {
        saw_digit = 1;
        len = (len * 10) + (size_t)(**cursor - '0');
        (*cursor)++;
    }

    if (!saw_digit || **cursor != ':') {
        return -1;
    }

    (*cursor)++;
    *out_len = len;
    return 0;
}

/* 저장된 행 1개를 다시 컬럼 값들로 분리합니다. */
static int parse_encoded_row(const char *line, int expected_columns,
                             char out_values[][MAX_VALUE_LEN]) {
    const char *cursor = line;
    int col;

    for (col = 0; col < expected_columns; col++) {
        size_t len;

        if (parse_next_length(&cursor, &len) != 0 || len >= MAX_VALUE_LEN) {
            return -1;
        }

        /* 길이를 알았으니 그 길이만큼 정확히 복사합니다. */
        if (strlen(cursor) < len) {
            return -1;
        }

        memcpy(out_values[col], cursor, len);
        out_values[col][len] = '\0';
        cursor += len;
    }

    while (*cursor == '\r' || *cursor == '\n') {
        cursor++;
    }

    return *cursor == '\0' ? 0 : -1;
}

/* <table>.data 파일 전체를 읽어서 스키마 순서의 ResultSet으로 만듭니다. */
static int load_full_table(FileStore *store, const TableDef *table, ResultSet *out) {
    char path[MAX_PATH_LEN];
    FILE *fp;
    char line[MAX_ROW_LINE_LEN];

    /* ResultSet을 먼저 비우고,
       헤더를 스키마 순서대로 채워서 "전체 테이블 모습"을 담을 준비를 합니다. */
    set_full_headers(table, out);

    if (build_table_path(store, table->name, path, sizeof(path)) != 0) {
        return ERR_FILE_OPEN;
    }

    /* 데이터 파일이 아직 없다면 "빈 테이블"로 보면 되므로 에러가 아닙니다. */
    fp = fopen(path, "r");
    if (fp == NULL) {
        if (errno == ENOENT) {
            return ERR_NONE;
        }
        return ERR_FILE_OPEN;
    }

    while (fgets(line, sizeof(line), fp) != NULL) {
        char row_values[MAX_COLUMNS][MAX_VALUE_LEN];
        int col;

        /* ResultSet은 고정 크기이므로 너무 많은 row는 더 담을 수 없습니다. */
        if (out->row_count >= MAX_ROWS) {
            fclose(fp);
            return ERR_INVALID_QUERY;
        }

        /* 파일에 저장된 길이:값 포맷을 다시 컬럼 배열로 복원합니다. */
        if (parse_encoded_row(line, table->column_count, row_values) != 0) {
            fclose(fp);
            return ERR_FILE_OPEN;
        }

        /* 복원한 row를 현재 row_count 위치에 옮긴 뒤,
           다음 줄은 그 다음 칸에 쌓이도록 row_count를 늘립니다. */
        for (col = 0; col < table->column_count; col++) {
            strcpy(out->rows[out->row_count][col], row_values[col]);
        }
        out->row_count++;
    }

    if (ferror(fp)) {
        fclose(fp);
        return ERR_FILE_OPEN;
    }

    fclose(fp);
    return ERR_NONE;
}

/* 전체 row를 다 읽은 뒤, SELECT가 요청한 컬럼만 남기도록 ResultSet을 줄입니다. */
static int project_result_in_place(ResultSet *result, const TableDef *table,
                                   const char columns[][MAX_NAME_LEN], int col_count,
                                   int select_all) {
    int selected_indices[MAX_COLUMNS];
    char projected_headers[MAX_COLUMNS][MAX_NAME_LEN];
    int row;
    int col;

    /* SELECT * 는 이미 전체 컬럼 상태이므로 손댈 필요가 없습니다. */
    if (select_all) {
        return ERR_NONE;
    }

    /* 요청한 컬럼 이름이 스키마에서 몇 번째인지 먼저 찾아 둡니다.
       이 인덱스를 써서 각 row에서 필요한 값만 골라낼 수 있습니다. */
    for (col = 0; col < col_count; col++) {
        selected_indices[col] = find_column_index(table, columns[col]);
        if (selected_indices[col] < 0) {
            return ERR_INVALID_QUERY;
        }
        strcpy(projected_headers[col], columns[col]);
    }

    for (row = 0; row < result->row_count; row++) {
        char projected_row[MAX_COLUMNS][MAX_VALUE_LEN];

        /* 한 row 안에서 요청한 컬럼만 임시 버퍼로 먼저 모읍니다. */
        for (col = 0; col < col_count; col++) {
            strcpy(projected_row[col], result->rows[row][selected_indices[col]]);
        }
        /* 그다음 원래 row 앞부분을 projection 결과로 덮어씁니다. */
        for (col = 0; col < col_count; col++) {
            strcpy(result->rows[row][col], projected_row[col]);
        }
    }

    /* 출력 헤더도 요청 컬럼 목록에 맞춰 같이 줄여 줍니다. */
    for (col = 0; col < col_count; col++) {
        strcpy(result->headers[col], projected_headers[col]);
    }
    result->header_count = col_count;
    return ERR_NONE;
}

/* 저장소를 처음 준비합니다.
   스키마 정보를 저장하고, .data 파일을 둘 디렉토리도 기억해 둡니다. */
static int file_init(void *ctx, const TableDef *tables, int table_count) {
    FileStore *store = (FileStore *)ctx;

    memset(store, 0, sizeof(*store));
    if (table_count > MAX_TABLES) {
        return ERR_INVALID_QUERY;
    }

    if (getcwd(store->data_dir, sizeof(store->data_dir)) == NULL) {
        return ERR_FILE_OPEN;
    }

    store->table_count = table_count;
    memcpy(store->tables, tables, (size_t)table_count * sizeof(TableDef));
    return ERR_NONE;
}

/* INSERT를 처리할 때는 <table>.data 파일 뒤에 행 1개를 추가합니다. */
static int file_insert(void *ctx, const char *table_name,
                       const char values[][MAX_VALUE_LEN], int value_count) {
    FileStore *store = (FileStore *)ctx;
    const TableDef *table = find_table(store, table_name);
    char path[MAX_PATH_LEN];
    FILE *fp;

    if (table == NULL) {
        return ERR_TABLE_NOT_FOUND;
    }

    if (value_count != table->column_count) {
        return ERR_COLUMN_MISMATCH;
    }

    if (build_table_path(store, table_name, path, sizeof(path)) != 0) {
        return ERR_FILE_OPEN;
    }

    /* append 모드로 열어야 새 행이 파일 맨 뒤에 붙습니다. */
    fp = fopen(path, "a");
    if (fp == NULL) {
        return ERR_FILE_OPEN;
    }

    if (write_encoded_row(fp, values, value_count) != 0) {
        fclose(fp);
        return ERR_FILE_OPEN;
    }

    fclose(fp);
    return ERR_NONE;
}

/* 테이블 전체 row를 읽어오는 StorageOps 함수입니다.
   executor가 WHERE, ORDER BY, UPDATE, DELETE를 처리할 때 사용합니다.
   Phase 2부터는 SELECT도 필요하면 이 함수를 통해 full-row 기준 작업을 할 수 있습니다. */
static int file_read_all_rows(void *ctx, const char *table_name, ResultSet *out) {
    FileStore *store = (FileStore *)ctx;
    const TableDef *table = find_table(store, table_name);

    if (table == NULL) {
        return ERR_TABLE_NOT_FOUND;
    }

    return load_full_table(store, table, out);
}

/* 바뀐 전체 row를 파일에 다시 씁니다.
   DELETE나 UPDATE처럼 파일 전체를 다시 만들어야 할 때 사용합니다. */
static int file_replace_rows(void *ctx, const char *table_name, const ResultSet *rows) {
    FileStore *store = (FileStore *)ctx;
    const TableDef *table = find_table(store, table_name);
    char path[MAX_PATH_LEN];
    FILE *fp;
    int row;

    if (table == NULL) {
        return ERR_TABLE_NOT_FOUND;
    }
    /* replace_rows는 "테이블 전체 모습"을 다시 쓰는 함수이므로
       header 수가 있다면 schema 전체 컬럼 수와 맞아야 합니다. */
    if (rows->header_count != 0 && rows->header_count != table->column_count) {
        return ERR_INVALID_QUERY;
    }
    if (build_table_path(store, table_name, path, sizeof(path)) != 0) {
        return ERR_FILE_OPEN;
    }

    /* w 모드로 열면 기존 내용을 비우고 새 테이블 내용으로 덮어씁니다. */
    fp = fopen(path, "w");
    if (fp == NULL) {
        return ERR_FILE_OPEN;
    }

    for (row = 0; row < rows->row_count; row++) {
        /* 메모리 안의 row를 파일 포맷으로 다시 저장합니다. */
        if (write_encoded_row(fp, rows->rows[row], table->column_count) != 0) {
            fclose(fp);
            return ERR_FILE_OPEN;
        }
    }

    fclose(fp);
    return ERR_NONE;
}

/* SELECT를 처리할 때는 <table>.data 파일을 읽고,
   필요한 컬럼만 골라 ResultSet에 담습니다.
   즉 storage 계층 입장에서는 "파일 읽기 + 단순 projection"까지만 담당합니다. */
static int file_select_rows(void *ctx, const char *table_name,
                            const char columns[][MAX_NAME_LEN], int col_count,
                            int select_all, ResultSet *out) {
    FileStore *store = (FileStore *)ctx;
    const TableDef *table = find_table(store, table_name);
    int status;

    if (table == NULL) {
        return ERR_TABLE_NOT_FOUND;
    }

    /* 먼저 테이블 전체를 스키마 순서 그대로 읽어옵니다. */
    status = load_full_table(store, table, out);
    if (status != ERR_NONE) {
        return status;
    }

    /* SELECT가 특정 컬럼만 원하면 여기서 projection 합니다. */
    return project_result_in_place(out, table, columns, col_count, select_all);
}

/* 저장소에서 쓰던 메모리를 해제합니다. */
static void file_destroy(void *ctx) {
    free(ctx);
}

/* StorageOps vtable을 만들고,
   각 함수 포인터를 파일 저장소 구현에 연결합니다.
   parser/executor는 이 포인터들만 호출하므로,
   실제 저장 방식이 파일인지 다른 방식인지는 여기 아래에서만 결정됩니다. */
StorageOps *file_storage_create(void) {
    StorageOps *ops = (StorageOps *)malloc(sizeof(*ops));
    FileStore *store;

    if (ops == NULL) {
        return NULL;
    }

    store = (FileStore *)malloc(sizeof(*store));
    if (store == NULL) {
        free(ops);
        return NULL;
    }

    memset(store, 0, sizeof(*store));
    ops->ctx = store;
    ops->init = file_init;
    ops->insert = file_insert;
    ops->select_rows = file_select_rows;
    ops->read_all_rows = file_read_all_rows;
    ops->replace_rows = file_replace_rows;
    ops->destroy = file_destroy;
    return ops;
}
