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

/* SELECT를 처리할 때는 <table>.data 파일을 읽고,
   필요한 컬럼만 골라 ResultSet에 담습니다. */
static int file_select_rows(void *ctx, const char *table_name,
                            const char columns[][MAX_NAME_LEN], int col_count,
                            int select_all, ResultSet *out) {
    FileStore *store = (FileStore *)ctx;
    const TableDef *table = find_table(store, table_name);
    int selected_indices[MAX_COLUMNS];
    int selected_count = 0;
    char path[MAX_PATH_LEN];
    FILE *fp;
    char line[MAX_ROW_LINE_LEN];

    result_set_reset(out);

    if (table == NULL) {
        return ERR_TABLE_NOT_FOUND;
    }

    if (select_all) {
        int col;

        /* SELECT * 는 스키마에 있는 모든 컬럼을 순서대로 사용한다는 뜻입니다. */
        for (col = 0; col < table->column_count; col++) {
            selected_indices[selected_count] = col;
            strcpy(out->headers[selected_count], table->columns[col].name);
            selected_count++;
        }
    } else {
        int col;

        /* SELECT name,age 는 그 컬럼 위치만 기억해 두겠다는 뜻입니다. */
        for (col = 0; col < col_count; col++) {
            int index = find_column_index(table, columns[col]);

            if (index < 0) {
                return ERR_INVALID_QUERY;
            }

            selected_indices[selected_count] = index;
            strcpy(out->headers[selected_count], table->columns[index].name);
            selected_count++;
        }
    }

    out->header_count = selected_count;

    if (build_table_path(store, table_name, path, sizeof(path)) != 0) {
        return ERR_FILE_OPEN;
    }

    /* 데이터 파일이 아직 없으면 테이블은 있지만 아직 비어 있다고 봅니다. */
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

        /* ResultSet은 크기가 고정이라 너무 많은 행이 들어오면 막아야 합니다. */
        if (out->row_count >= MAX_ROWS) {
            fclose(fp);
            return ERR_INVALID_QUERY;
        }

        /* 저장된 행 전체를 먼저 해독한 뒤,
           필요한 컬럼만 골라 ResultSet으로 옮깁니다. */
        if (parse_encoded_row(line, table->column_count, row_values) != 0) {
            fclose(fp);
            return ERR_FILE_OPEN;
        }

        for (col = 0; col < selected_count; col++) {
            strcpy(out->rows[out->row_count][col], row_values[selected_indices[col]]);
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

/* 저장소에서 쓰던 메모리를 해제합니다. */
static void file_destroy(void *ctx) {
    free(ctx);
}

/* StorageOps vtable을 만들고,
   각 함수 포인터를 파일 저장소 구현에 연결합니다. */
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
    ops->destroy = file_destroy;
    return ops;
}
