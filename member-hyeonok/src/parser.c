#include "parser.h"

#include "util.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static void initialize_statement(SqlStatement *statement)
{
    statement->type = SQL_STATEMENT_UNCLASSIFIED;
    statement->raw_sql = NULL;
    statement->insert.table_name = NULL;
    statement->insert.columns = NULL;
    statement->insert.column_count = 0;
    statement->insert.values = NULL;
    statement->insert.value_count = 0;
    statement->select.select_all = 0;
    statement->select.table_name = NULL;
    statement->select.columns = NULL;
    statement->select.column_count = 0;
}

static void skip_spaces(const char **cursor)
{
    while (**cursor != '\0' && isspace((unsigned char)**cursor)) {
        (*cursor)++;
    }
}

static int is_identifier_char(char ch)
{
    return isalnum((unsigned char)ch) || ch == '_';
}

static int match_keyword(const char **cursor, const char *keyword)
{
    const char *source;
    size_t index;

    source = *cursor;
    skip_spaces(&source);

    for (index = 0; keyword[index] != '\0'; index++) {
        if (tolower((unsigned char)source[index]) !=
            tolower((unsigned char)keyword[index])) {
            return 0;
        }
    }

    if (is_identifier_char(source[index])) {
        return 0;
    }

    *cursor = source + index;
    return 1;
}

static char *copy_range_trimmed(const char *start, size_t length)
{
    char *buffer;
    char *trimmed;

    buffer = (char *)malloc(length + 1);
    if (buffer == NULL) {
        return NULL;
    }

    memcpy(buffer, start, length);
    buffer[length] = '\0';

    trimmed = trim_in_place(buffer);
    if (trimmed != buffer) {
        memmove(buffer, trimmed, strlen(trimmed) + 1);
    }

    return buffer;
}

static char *copy_exact_range(const char *start, size_t length)
{
    char *buffer;

    buffer = (char *)malloc(length + 1);
    if (buffer == NULL) {
        return NULL;
    }

    memcpy(buffer, start, length);
    buffer[length] = '\0';
    return buffer;
}

static char *parse_identifier_token(const char **cursor)
{
    const char *start;

    skip_spaces(cursor);
    start = *cursor;

    if (!is_identifier_char(*start)) {
        return NULL;
    }

    while (is_identifier_char(**cursor)) {
        (*cursor)++;
    }

    return copy_range_trimmed(start, (size_t)(*cursor - start));
}

static int append_column(InsertStatement *insert, char *column_name)
{
    char **new_columns;

    new_columns = (char **)realloc(
        insert->columns,
        sizeof(char *) * (insert->column_count + 1)
    );
    if (new_columns == NULL) {
        return 0;
    }

    insert->columns = new_columns;
    insert->columns[insert->column_count] = column_name;
    insert->column_count++;
    return 1;
}

static int append_select_column(SelectStatement *select, char *column_name)
{
    char **new_columns;

    new_columns = (char **)realloc(
        select->columns,
        sizeof(char *) * (select->column_count + 1)
    );
    if (new_columns == NULL) {
        return 0;
    }

    select->columns = new_columns;
    select->columns[select->column_count] = column_name;
    select->column_count++;
    return 1;
}

static int append_value(InsertStatement *insert, const SqlValue *value)
{
    SqlValue *new_values;

    new_values = (SqlValue *)realloc(
        insert->values,
        sizeof(SqlValue) * (insert->value_count + 1)
    );
    if (new_values == NULL) {
        return 0;
    }

    insert->values = new_values;
    insert->values[insert->value_count] = *value;
    insert->value_count++;
    return 1;
}

static int parse_column_list(const char **cursor, InsertStatement *insert)
{
    char *column_name;

    skip_spaces(cursor);
    if (**cursor != '(') {
        return 0;
    }
    (*cursor)++;

    while (1) {
        column_name = parse_identifier_token(cursor);
        if (column_name == NULL) {
            return 0;
        }

        if (!append_column(insert, column_name)) {
            free(column_name);
            return 0;
        }

        skip_spaces(cursor);
        if (**cursor == ',') {
            (*cursor)++;
            continue;
        }

        if (**cursor == ')') {
            (*cursor)++;
            break;
        }

        return 0;
    }

    return insert->column_count > 0;
}

static int parse_string_value(const char **cursor, SqlValue *value)
{
    const char *start;
    char *text;

    if (**cursor != '\'') {
        return 0;
    }
    (*cursor)++;
    start = *cursor;

    while (**cursor != '\0' && **cursor != '\'') {
        (*cursor)++;
    }

    if (**cursor != '\'') {
        return 0;
    }

    /*
     * 작은따옴표 내부 문자열은 사용자가 적은 내용을 그대로 보존한다.
     * 값 앞뒤 공백도 데이터 일부일 수 있으므로 별도 trim 을 하지 않는다.
     */
    text = copy_exact_range(start, (size_t)(*cursor - start));
    if (text == NULL) {
        return 0;
    }

    value->type = SQL_VALUE_STRING;
    value->text = text;
    value->int_value = 0;
    (*cursor)++;
    return 1;
}

static int parse_int_value(const char **cursor, SqlValue *value)
{
    const char *start;
    char *text;
    char *end_ptr;
    long parsed;

    start = *cursor;
    if (**cursor == '-' || **cursor == '+') {
        (*cursor)++;
    }

    if (!isdigit((unsigned char)**cursor)) {
        return 0;
    }

    while (isdigit((unsigned char)**cursor)) {
        (*cursor)++;
    }

    text = copy_range_trimmed(start, (size_t)(*cursor - start));
    if (text == NULL) {
        return 0;
    }

    parsed = strtol(text, &end_ptr, 10);
    if (*end_ptr != '\0') {
        free(text);
        return 0;
    }

    value->type = SQL_VALUE_INT;
    value->text = text;
    value->int_value = (int)parsed;
    return 1;
}

static int parse_value_item(const char **cursor, SqlValue *value)
{
    skip_spaces(cursor);

    if (**cursor == '\'') {
        return parse_string_value(cursor, value);
    }

    return parse_int_value(cursor, value);
}

static int parse_value_list(const char **cursor, InsertStatement *insert)
{
    SqlValue value;

    skip_spaces(cursor);
    if (**cursor != '(') {
        return 0;
    }
    (*cursor)++;

    while (1) {
        value.type = SQL_VALUE_STRING;
        value.text = NULL;
        value.int_value = 0;

        if (!parse_value_item(cursor, &value)) {
            return 0;
        }

        if (!append_value(insert, &value)) {
            free(value.text);
            return 0;
        }

        skip_spaces(cursor);
        if (**cursor == ',') {
            (*cursor)++;
            continue;
        }

        if (**cursor == ')') {
            (*cursor)++;
            break;
        }

        return 0;
    }

    return insert->value_count > 0;
}

static int parse_insert_statement(const char *sql_text, SqlStatement *statement)
{
    const char *cursor;
    char *table_name;

    cursor = sql_text;

    /*
     * 과제에서 요구하는 INSERT 형태만 정확히 지원한다.
     * 키워드는 대소문자를 무시하고, 공백은 유연하게 허용한다.
     */
    if (!match_keyword(&cursor, "INSERT")) {
        return 0;
    }
    if (!match_keyword(&cursor, "INTO")) {
        return 0;
    }

    table_name = parse_identifier_token(&cursor);
    if (table_name == NULL) {
        return 0;
    }
    statement->insert.table_name = table_name;

    if (!parse_column_list(&cursor, &statement->insert)) {
        return 0;
    }

    if (!match_keyword(&cursor, "VALUES")) {
        return 0;
    }

    if (!parse_value_list(&cursor, &statement->insert)) {
        return 0;
    }

    skip_spaces(&cursor);
    if (*cursor != '\0') {
        return 0;
    }

    statement->type = SQL_STATEMENT_INSERT;
    return 1;
}

static int parse_select_list(const char **cursor, SelectStatement *select)
{
    char *column_name;

    skip_spaces(cursor);
    if (**cursor == '*') {
        /*
         * SELECT * 는 컬럼 배열을 채우지 않고 플래그만 세워서
         * 이후 executor 가 스키마 전체 컬럼을 사용하게 만든다.
         */
        select->select_all = 1;
        (*cursor)++;
        return 1;
    }

    while (1) {
        column_name = parse_identifier_token(cursor);
        if (column_name == NULL) {
            return 0;
        }

        if (!append_select_column(select, column_name)) {
            free(column_name);
            return 0;
        }

        skip_spaces(cursor);
        if (**cursor == ',') {
            (*cursor)++;
            continue;
        }

        break;
    }

    return select->column_count > 0;
}

static int parse_select_statement(const char *sql_text, SqlStatement *statement)
{
    const char *cursor;
    char *table_name;

    cursor = sql_text;

    /*
     * 현재 단계에서는 SELECT * 와 SELECT 컬럼목록만 지원한다.
     * WHERE, ORDER BY 같은 추가 문법은 일부러 받지 않는다.
     */
    if (!match_keyword(&cursor, "SELECT")) {
        return 0;
    }

    if (!parse_select_list(&cursor, &statement->select)) {
        return 0;
    }

    if (!match_keyword(&cursor, "FROM")) {
        return 0;
    }

    table_name = parse_identifier_token(&cursor);
    if (table_name == NULL) {
        return 0;
    }
    statement->select.table_name = table_name;

    skip_spaces(&cursor);
    if (*cursor != '\0') {
        return 0;
    }

    statement->type = SQL_STATEMENT_SELECT;
    return 1;
}

ParseResult parse_query(const char *sql_text, SqlStatement *statement)
{
    const char *cursor;

    if (sql_text == NULL || statement == NULL) {
        return PARSE_INTERNAL_ERROR;
    }

    initialize_statement(statement);
    statement->raw_sql = duplicate_string(sql_text);
    if (statement->raw_sql == NULL) {
        return PARSE_INTERNAL_ERROR;
    }

    cursor = sql_text;
    if (match_keyword(&cursor, "INSERT")) {
        cursor = sql_text;
        if (!parse_insert_statement(cursor, statement)) {
            free_sql_statement(statement);
            return PARSE_INVALID_QUERY;
        }
        return PARSE_SUCCESS;
    }

    cursor = sql_text;
    if (match_keyword(&cursor, "SELECT")) {
        cursor = sql_text;
        if (!parse_select_statement(cursor, statement)) {
            free_sql_statement(statement);
            return PARSE_INVALID_QUERY;
        }
        return PARSE_SUCCESS;
    }

    /*
     * 현재 지원하는 문장은 INSERT 와 SELECT 뿐이므로,
     * 시작 키워드를 판별하지 못하면 곧바로 invalid query 로 본다.
     */
    free_sql_statement(statement);
    return PARSE_INVALID_QUERY;
}

int parse_sql_statement(const char *sql_text, SqlStatement *statement)
{
    return parse_query(sql_text, statement) == PARSE_SUCCESS;
}

void free_sql_statement(SqlStatement *statement)
{
    size_t index;

    if (statement == NULL) {
        return;
    }

    free(statement->insert.table_name);
    statement->insert.table_name = NULL;

    if (statement->insert.columns != NULL) {
        for (index = 0; index < statement->insert.column_count; index++) {
            free(statement->insert.columns[index]);
        }
        free(statement->insert.columns);
    }
    statement->insert.columns = NULL;
    statement->insert.column_count = 0;

    if (statement->insert.values != NULL) {
        for (index = 0; index < statement->insert.value_count; index++) {
            free(statement->insert.values[index].text);
        }
        free(statement->insert.values);
    }
    statement->insert.values = NULL;
    statement->insert.value_count = 0;

    free(statement->select.table_name);
    statement->select.table_name = NULL;

    if (statement->select.columns != NULL) {
        for (index = 0; index < statement->select.column_count; index++) {
            free(statement->select.columns[index]);
        }
        free(statement->select.columns);
    }
    statement->select.columns = NULL;
    statement->select.column_count = 0;
    statement->select.select_all = 0;

    free(statement->raw_sql);
    statement->raw_sql = NULL;
    statement->type = SQL_STATEMENT_UNCLASSIFIED;
}
