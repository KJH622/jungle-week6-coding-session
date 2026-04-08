#ifndef TYPES_H
#define TYPES_H

#define MAX_NAME_LEN 256
#define MAX_VALUE_LEN 256
#define MAX_COLUMNS 32
#define MAX_VALUES 32
#define MAX_ROWS 1024
#define MAX_TABLES 16
#define MAX_SET_CLAUSES 32
/* UPDATE ... SET ... 에서 바꿀 컬럼 개수의 최대치입니다.
   예: SET age = 20, major = '수학' 처럼 여러 개를 담을 수 있습니다. */

/* 파싱한 SQL 문장이 어떤 종류인지 저장합니다.
   parser는 먼저 이 종류를 정하고,
   executor는 이 값을 보고 어떤 실행 함수를 탈지 결정합니다. */
typedef enum {
    CMD_NONE,
    CMD_INSERT,
    CMD_SELECT,
    CMD_DELETE,
    CMD_UPDATE
} CommandType;

/* WHERE 절에서 어떤 비교를 할지 나타냅니다.
   예를 들어 age >= 20 이면 OP_GE가 들어갑니다. */
typedef enum {
    OP_EQ,
    OP_NE,
    OP_GT,
    OP_LT,
    OP_GE,
    OP_LE
} CompareOp;

/* WHERE 절의 조건 1개를 저장합니다.
   이번 프로젝트는 AND/OR 없이 조건 1개만 지원하므로
   컬럼 이름, 비교 연산자, 비교값 3가지만 있으면 됩니다. */
typedef struct {
    char column[MAX_NAME_LEN];
    CompareOp op;
    char value[MAX_VALUE_LEN];
} WhereCondition;

/* ORDER BY 방향입니다.
   방향을 안 적으면 기본값은 ORDER_ASC입니다. */
typedef enum {
    ORDER_ASC,
    ORDER_DESC
} OrderDirection;

/* ORDER BY 절 정보를 저장합니다.
   어떤 컬럼으로, 어떤 방향으로 정렬할지 기억합니다. */
typedef struct {
    char column[MAX_NAME_LEN];
    OrderDirection direction;
} OrderByClause;

/* UPDATE의 SET 절에서 컬럼=값 한 쌍을 저장합니다.
   UPDATE는 여러 컬럼을 한 번에 바꿀 수 있으므로
   이런 쌍을 배열로 여러 개 들고 있게 됩니다. */
typedef struct {
    char column[MAX_NAME_LEN];
    char value[MAX_VALUE_LEN];
} SetClause;

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

/* SQL 문장 1개를 파싱한 결과를 담는 구조체입니다.
   parser는 SQL 문자열을 이 구조체로 "번역"해 두고,
   executor는 문자열을 다시 읽지 않고 이 구조체만 보고 실행합니다. */
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

    /* WHERE는 SELECT, DELETE, UPDATE에서 공통으로 사용합니다.
       has_where가 1이면 where 안의 정보를 실제로 사용합니다. */
    int has_where;
    WhereCondition where;

    /* ORDER BY와 LIMIT은 SELECT에서 사용합니다.
       둘 다 선택 기능이므로 has_* 값으로 "있는지 여부"를 먼저 확인합니다. */
    int has_order_by;
    OrderByClause order_by;
    int has_limit;
    int limit_count;

    /* UPDATE는 SET 절이 여러 개 올 수 있습니다.
       set_clause_count 만큼만 실제 데이터가 들어 있다고 보면 됩니다. */
    SetClause set_clauses[MAX_SET_CLAUSES];
    int set_clause_count;
} Command;

/* SELECT 결과를 담아 두었다가 나중에 출력할 때 사용합니다.
   중요한 점은 이 구조체가 "출력 직전 결과"로도 쓰이고,
   Phase 2부터는 "테이블 전체 row를 잠깐 들고 있는 작업 버퍼"로도 쓰인다는 것입니다. */
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
    /* 고급 기능을 위해 테이블 전체 row를 읽습니다.
       SELECT의 WHERE/ORDER BY/LIMIT, DELETE, UPDATE는
       먼저 전체 row를 읽어 와야 안전하게 처리할 수 있습니다. */
    int (*read_all_rows)(void *ctx, const char *table, ResultSet *out);
    /* DELETE/UPDATE 후 바뀐 전체 row를 파일에 다시 씁니다.
       조건에 맞는 row를 지우거나 수정한 뒤에는
       바뀐 테이블 전체 모습을 파일에 다시 써야 합니다. */
    int (*replace_rows)(void *ctx, const char *table, const ResultSet *rows);
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
