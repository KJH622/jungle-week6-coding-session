#include "executor.h"

#include "schema.h"
#include "storage.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int find_schema_column_index(const Schema *schema, const char *column_name)
{
    size_t index;

    for (index = 0; index < schema->column_count; index++) {
        if (strcmp(schema->columns[index].name, column_name) == 0) {
            return (int)index;
        }
    }

    return -1;
}

static ExecuteResult append_insert_row(
    const InsertStatement *insert,
    const Schema *schema
)
{
    const SqlValue **ordered_values;
    const char **ordered_text_values;
    size_t insert_index;
    size_t schema_index;
    int found_index;
    StorageResult storage_result;

    ordered_values = (const SqlValue **)calloc(schema->column_count, sizeof(SqlValue *));
    if (ordered_values == NULL) {
        return EXECUTE_FILE_ERROR;
    }

    /*
     * INSERT 컬럼 순서가 스키마와 달라도 저장 파일은 항상 스키마 순서로 맞춘다.
     * 이렇게 두면 이후 SELECT 구현에서 헤더와 데이터 행을 같은 기준으로 읽을 수 있다.
     */
    for (insert_index = 0; insert_index < insert->column_count; insert_index++) {
        found_index = find_schema_column_index(schema, insert->columns[insert_index]);
        if (found_index < 0) {
            free(ordered_values);
            return EXECUTE_INVALID_QUERY;
        }

        schema_index = (size_t)found_index;
        if (ordered_values[schema_index] != NULL) {
            free(ordered_values);
            return EXECUTE_INVALID_QUERY;
        }

        ordered_values[schema_index] = &insert->values[insert_index];
    }

    for (schema_index = 0; schema_index < schema->column_count; schema_index++) {
        if (ordered_values[schema_index] == NULL) {
            free(ordered_values);
            return EXECUTE_INVALID_QUERY;
        }
    }

    ordered_text_values = (const char **)calloc(schema->column_count, sizeof(char *));
    if (ordered_text_values == NULL) {
        free(ordered_values);
        return EXECUTE_FILE_ERROR;
    }

    for (schema_index = 0; schema_index < schema->column_count; schema_index++) {
        ordered_text_values[schema_index] = ordered_values[schema_index]->text;
    }

    storage_result = append_storage_row(
        insert->table_name,
        ordered_text_values,
        schema->column_count
    );
    free(ordered_text_values);
    free(ordered_values);

    if (storage_result != STORAGE_SUCCESS) {
        return EXECUTE_FILE_ERROR;
    }

    return EXECUTE_SUCCESS;
}

static ExecuteResult execute_insert_statement(const InsertStatement *insert)
{
    Schema schema;
    SchemaLoadResult schema_result;
    ExecuteResult result;

    schema_result = load_schema_for_table(insert->table_name, &schema);
    if (schema_result == SCHEMA_LOAD_TABLE_NOT_FOUND) {
        return EXECUTE_TABLE_NOT_FOUND;
    }

    if (schema_result != SCHEMA_LOAD_SUCCESS) {
        return EXECUTE_FILE_ERROR;
    }

    /*
     * 과제 명세상 가장 먼저 보장해야 하는 검사는 컬럼 수와 값 수의 일치 여부다.
     * 이 조건이 맞지 않으면 데이터 파일에는 아무것도 기록하지 않는다.
     */
    if (insert->column_count != insert->value_count) {
        free_schema(&schema);
        return EXECUTE_COLUMN_COUNT_MISMATCH;
    }

    if (insert->column_count != schema.column_count) {
        free_schema(&schema);
        return EXECUTE_COLUMN_COUNT_MISMATCH;
    }

    result = append_insert_row(insert, &schema);
    free_schema(&schema);
    return result;
}

static int *build_select_column_indexes(
    const SelectStatement *select,
    const Schema *schema,
    size_t *selected_count,
    ExecuteResult *result
)
{
    int *indexes;
    size_t index;
    int found_index;

    if (select->select_all) {
        indexes = (int *)malloc(sizeof(int) * schema->column_count);
        if (indexes == NULL) {
            *result = EXECUTE_FILE_ERROR;
            return NULL;
        }

        for (index = 0; index < schema->column_count; index++) {
            indexes[index] = (int)index;
        }

        *selected_count = schema->column_count;
        *result = EXECUTE_SUCCESS;
        return indexes;
    }

    indexes = (int *)malloc(sizeof(int) * select->column_count);
    if (indexes == NULL) {
        *result = EXECUTE_FILE_ERROR;
        return NULL;
    }

    for (index = 0; index < select->column_count; index++) {
        found_index = find_schema_column_index(schema, select->columns[index]);
        if (found_index < 0) {
            free(indexes);
            *result = EXECUTE_INVALID_QUERY;
            return NULL;
        }
        indexes[index] = found_index;
    }

    *selected_count = select->column_count;
    *result = EXECUTE_SUCCESS;
    return indexes;
}

static int print_selected_header(
    const SelectStatement *select,
    const Schema *schema,
    const int *indexes,
    size_t selected_count
)
{
    size_t index;

    for (index = 0; index < selected_count; index++) {
        if (index > 0 && fputc(',', stdout) == EOF) {
            return 0;
        }

        if (select->select_all) {
            if (fputs(schema->columns[indexes[index]].name, stdout) == EOF) {
                return 0;
            }
        } else {
            if (fputs(select->columns[index], stdout) == EOF) {
                return 0;
            }
        }
    }

    return fputc('\n', stdout) != EOF;
}

static int print_selected_row(
    char **fields,
    const int *indexes,
    size_t selected_count
)
{
    size_t index;

    for (index = 0; index < selected_count; index++) {
        if (index > 0 && fputc(',', stdout) == EOF) {
            return 0;
        }

        if (fputs(fields[indexes[index]], stdout) == EOF) {
            return 0;
        }
    }

    return fputc('\n', stdout) != EOF;
}

static ExecuteResult execute_select_statement(const SelectStatement *select)
{
    Schema schema;
    SchemaLoadResult schema_result;
    char *data_text;
    char *cursor;
    int *selected_indexes;
    size_t selected_count;
    int printed_header;
    ExecuteResult result;
    StorageResult storage_result;

    schema_result = load_schema_for_table(select->table_name, &schema);
    if (schema_result == SCHEMA_LOAD_TABLE_NOT_FOUND) {
        return EXECUTE_TABLE_NOT_FOUND;
    }

    if (schema_result != SCHEMA_LOAD_SUCCESS) {
        return EXECUTE_FILE_ERROR;
    }

    selected_indexes = build_select_column_indexes(
        select,
        &schema,
        &selected_count,
        &result
    );
    if (selected_indexes == NULL) {
        free_schema(&schema);
        return result;
    }

    storage_result = load_storage_text(select->table_name, &data_text);
    if (storage_result == STORAGE_NOT_FOUND) {
        free(selected_indexes);
        free_schema(&schema);
        return EXECUTE_SUCCESS;
    }

    if (storage_result != STORAGE_SUCCESS) {
        free(selected_indexes);
        free_schema(&schema);
        return EXECUTE_FILE_ERROR;
    }

    printed_header = 0;
    result = EXECUTE_SUCCESS;
    cursor = data_text;

    while (*cursor != '\0') {
        char *line_start;
        char *line_end;
        char saved_char;
        char **fields;

        line_start = cursor;
        line_end = strchr(cursor, '\n');
        if (line_end == NULL) {
            line_end = cursor + strlen(cursor);
            saved_char = *line_end;
        } else {
            saved_char = *line_end;
            *line_end = '\0';
        }

        /*
         * 빈 데이터 파일이거나 마지막 빈 줄은 출력 없이 건너뛴다.
         * 실제 row가 하나라도 있을 때만 헤더를 먼저 출력한다.
         */
        if (*line_start != '\0') {
            storage_result = parse_storage_row(
                line_start,
                schema.column_count,
                &fields
            );
            if (storage_result != STORAGE_SUCCESS) {
                result = EXECUTE_FILE_ERROR;
            } else {
                if (!printed_header) {
                    if (!print_selected_header(select, &schema, selected_indexes, selected_count)) {
                        free_string_array(fields, schema.column_count);
                        result = EXECUTE_FILE_ERROR;
                    } else {
                        printed_header = 1;
                    }
                }

                if (result == EXECUTE_SUCCESS &&
                    !print_selected_row(fields, selected_indexes, selected_count)) {
                    result = EXECUTE_FILE_ERROR;
                }

                free_string_array(fields, schema.column_count);
            }
        }

        if (saved_char == '\n') {
            *line_end = '\n';
            cursor = line_end + 1;
        } else {
            cursor = line_end;
        }

        if (result != EXECUTE_SUCCESS) {
            break;
        }
    }

    free(data_text);
    free(selected_indexes);
    free_schema(&schema);
    return result;
}

ExecuteResult execute_query(const SqlStatement *statement)
{
    if (statement == NULL) {
        return EXECUTE_FILE_ERROR;
    }

    if (statement->type == SQL_STATEMENT_INSERT) {
        return execute_insert_statement(&statement->insert);
    }

    if (statement->type == SQL_STATEMENT_SELECT) {
        return execute_select_statement(&statement->select);
    }

    return EXECUTE_INVALID_QUERY;
}

ExecuteResult execute_sql_statement(const SqlStatement *statement)
{
    return execute_query(statement);
}
