#include "include/schema.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "include/buffer_cache.h"
#include "include/utils.h"

/* 스키마 파일 전체를 읽어 텍스트 버퍼로 반환한다. */
static char *read_all_text(const char *path, SqlStatus *status)
{
    FILE *file;
    long size;
    size_t read_count;
    char *buffer;

    file = fopen(path, "rb");
    if (file == NULL) {
        *status = (errno == ENOENT) ? SQL_STATUS_TABLE_NOT_FOUND : SQL_STATUS_FILE_OPEN_FAILED;
        return NULL;
    }

    if (fseek(file, 0L, SEEK_END) != 0) {
        fclose(file);
        *status = SQL_STATUS_FILE_OPEN_FAILED;
        return NULL;
    }

    size = ftell(file);
    if (size < 0L) {
        fclose(file);
        *status = SQL_STATUS_FILE_OPEN_FAILED;
        return NULL;
    }

    if (fseek(file, 0L, SEEK_SET) != 0) {
        fclose(file);
        *status = SQL_STATUS_FILE_OPEN_FAILED;
        return NULL;
    }

    buffer = (char *)malloc((size_t)size + 1U);
    if (buffer == NULL) {
        fclose(file);
        *status = SQL_STATUS_INTERNAL_ERROR;
        return NULL;
    }

    read_count = fread(buffer, 1U, (size_t)size, file);
    fclose(file);

    if (read_count != (size_t)size) {
        free(buffer);
        *status = SQL_STATUS_FILE_OPEN_FAILED;
        return NULL;
    }

    buffer[size] = '\0';
    *status = SQL_STATUS_OK;
    return buffer;
}

/* JSON 유사 텍스트에서 지정한 키의 시작 위치를 찾는다. */
static const char *find_json_key(const char *cursor, const char *key)
{
    const char *scan;
    size_t key_length;

    scan = cursor;
    key_length = strlen(key);

    while ((scan = strstr(scan, key)) != NULL) {
        const char *after_key;

        after_key = scan + key_length;
        while (*after_key != '\0' && isspace((unsigned char)*after_key)) {
            after_key++;
        }

        if (*after_key == ':') {
            return scan;
        }

        scan += key_length;
    }

    return NULL;
}

/* 키 뒤에 오는 문자열 값을 추출해 새 메모리로 반환한다. */
static char *extract_string_after_key(const char *cursor, const char *key)
{
    const char *key_pos;
    const char *colon;
    const char *first_quote;
    const char *second_quote;

    key_pos = find_json_key(cursor, key);
    if (key_pos == NULL) {
        return NULL;
    }

    colon = strchr(key_pos + strlen(key), ':');
    if (colon == NULL) {
        return NULL;
    }

    first_quote = strchr(colon, '"');
    if (first_quote == NULL) {
        return NULL;
    }

    second_quote = strchr(first_quote + 1, '"');
    if (second_quote == NULL) {
        return NULL;
    }

    return util_strndup(first_quote + 1, (size_t)(second_quote - first_quote - 1));
}

/* 컬럼 배열의 name/type 문자열과 배열 자체를 해제한다. */
static void free_columns(SchemaColumn *columns, size_t count)
{
    size_t index;

    if (columns == NULL) {
        return;
    }

    for (index = 0U; index < count; index++) {
        free(columns[index].name);
        free(columns[index].type);
    }

    free(columns);
}

/* .schema 파일을 읽어 테이블명과 컬럼 목록을 메모리 구조체로 로드한다. */
SqlStatus schema_load(const char *schema_dir, const char *table_name, TableSchema *out_schema)
{
    char path[MAX_PATH_LENGTH];
    char *content;
    const char *columns_section;
    const char *cursor;
    size_t column_count;
    SchemaColumn *columns;
    char *parsed_table_name;
    SqlStatus status;

    memset(out_schema, 0, sizeof(*out_schema));
    snprintf(path, sizeof(path), "%s/%s.schema", schema_dir, table_name);

    content = read_all_text(path, &status);
    if (content == NULL) {
        return status;
    }

    parsed_table_name = extract_string_after_key(content, "\"table\"");
    if (parsed_table_name == NULL) {
        free(content);
        return SQL_STATUS_INVALID_QUERY;
    }
    util_lowercase_inplace(parsed_table_name);

    columns_section = strstr(content, "\"columns\"");
    if (columns_section == NULL) {
        free(parsed_table_name);
        free(content);
        return SQL_STATUS_INVALID_QUERY;
    }

    column_count = 0U;
    cursor = columns_section;
    while ((cursor = find_json_key(cursor, "\"name\"")) != NULL) {
        column_count++;
        cursor += strlen("\"name\"");
    }

    columns = (SchemaColumn *)calloc(column_count, sizeof(SchemaColumn));
    if (columns == NULL) {
        free(parsed_table_name);
        free(content);
        return SQL_STATUS_INTERNAL_ERROR;
    }

    cursor = columns_section;
    column_count = 0U;
    while ((cursor = find_json_key(cursor, "\"name\"")) != NULL) {
        const char *type_key;

        columns[column_count].name = extract_string_after_key(cursor, "\"name\"");
        type_key = find_json_key(cursor, "\"type\"");
        if (columns[column_count].name == NULL || type_key == NULL) {
            free_columns(columns, column_count + 1U);
            free(parsed_table_name);
            free(content);
            return SQL_STATUS_INVALID_QUERY;
        }

        columns[column_count].type = extract_string_after_key(type_key, "\"type\"");
        if (columns[column_count].type == NULL) {
            free_columns(columns, column_count + 1U);
            free(parsed_table_name);
            free(content);
            return SQL_STATUS_INVALID_QUERY;
        }

        util_lowercase_inplace(columns[column_count].name);
        util_lowercase_inplace(columns[column_count].type);
        column_count++;
        cursor = type_key + strlen("\"type\"");
    }

    out_schema->table_name = parsed_table_name;
    out_schema->column_count = column_count;
    out_schema->columns = columns;

    free(content);
    return SQL_STATUS_OK;
}

/* 컬럼명을 스키마에서 찾아 인덱스를 반환하고 없으면 -1을 반환한다. */
int schema_find_column(const TableSchema *schema, const char *column_name)
{
    size_t index;

    for (index = 0U; index < schema->column_count; index++) {
        if (util_case_equal(schema->columns[index].name, column_name)) {
            return (int)index;
        }
    }

    return -1;
}

/* 로드된 스키마 메모리를 정리하고 구조체를 초기화한다. */
void schema_destroy(TableSchema *schema)
{
    if (schema == NULL) {
        return;
    }

    free(schema->table_name);
    free_columns(schema->columns, schema->column_count);
    memset(schema, 0, sizeof(*schema));
}
