#ifndef TYPES_H
#define TYPES_H

#define MAX_NAME_LEN 256
#define MAX_VALUE_LEN 256
#define MAX_COLUMNS 32
#define MAX_VALUES 32
#define MAX_ROWS 1024
#define MAX_TABLES 16

/* 파싱한 SQL 문장이 어떤 종류인지 저장합니다. */
typedef enum {
    CMD_NONE,
    CMD_INSERT,
    CMD_SELECT
} CommandType;

/* 테이블 스키마 안의 컬럼 1개를 나타냅니다. 예: name:string */
typedef struct {
    char name[MAX_NAME_LEN];
    char type[MAX_NAME_LEN];
} ColumnDef;

/* 테이블 1개의 전체 스키마 정보입니다. */
typedef struct {
    char name[MAX_NAME_LEN];
    ColumnDef columns[MAX_COLUMNS];
    int column_count;
} TableDef;

/* SQL 문장 1개를 파싱한 결과를 담는 구조체입니다. */
typedef struct {
    CommandType type;
    char table_name[MAX_NAME_LEN];
    /* INSERT는 컬럼 목록과 실제 값이 둘 다 필요합니다. */
    char insert_columns[MAX_COLUMNS][MAX_NAME_LEN];
    int insert_column_count;
    char values[MAX_VALUES][MAX_VALUE_LEN];
    int value_count;
    /* SELECT는 특정 컬럼 목록 또는 전체 선택 여부가 필요합니다. */
    char columns[MAX_COLUMNS][MAX_NAME_LEN];
    int column_count;
    int is_select_all;
} Command;

/* SELECT 결과를 담아 두었다가 나중에 출력할 때 사용합니다. */
typedef struct {
    char headers[MAX_COLUMNS][MAX_NAME_LEN];
    int header_count;
    char rows[MAX_ROWS][MAX_COLUMNS][MAX_VALUE_LEN];
    int row_count;
} ResultSet;

/* 저장소 계층의 함수 모음표(vtable)입니다.
   parser/executor는 이 인터페이스만 사용하므로,
   나중에 저장 방식을 바꿔도 나머지 코드를 크게 안 고쳐도 됩니다. */
typedef struct StorageOps {
    void *ctx;
    int (*init)(void *ctx, const TableDef *tables, int table_count);
    int (*insert)(void *ctx, const char *table, const char values[][MAX_VALUE_LEN], int value_count);
    int (*select_rows)(void *ctx, const char *table,
                       const char columns[][MAX_NAME_LEN], int col_count,
                       int select_all, ResultSet *out);
    void (*destroy)(void *ctx);
} StorageOps;

/* 사용자에게 보여줄 메시지로 바꾸기 전의 내부 에러 코드입니다. */
typedef enum {
    ERR_NONE = 0,
    ERR_INVALID_QUERY,
    ERR_TABLE_NOT_FOUND,
    ERR_COLUMN_MISMATCH,
    ERR_FILE_OPEN
} ErrorCode;

#endif
