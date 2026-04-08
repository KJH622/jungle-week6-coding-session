#include "sql_splitter.h"

#include "util.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static int append_statement(SqlStatementList *list, const char *start, size_t length)
{
    char *buffer;
    char *trimmed;
    char **new_items;

    buffer = (char *)malloc(length + 1);
    if (buffer == NULL) {
        return 0;
    }

    memcpy(buffer, start, length);
    buffer[length] = '\0';

    /* 문장 경계에서 잘라낸 뒤 앞뒤 공백을 제거하고, 빈 문장은 저장하지 않는다. */
    trimmed = trim_in_place(buffer);
    if (*trimmed == '\0') {
        free(buffer);
        return 1;
    }

    if (trimmed != buffer) {
        memmove(buffer, trimmed, strlen(trimmed) + 1);
    }

    new_items = (char **)realloc(list->items, sizeof(char *) * (list->count + 1));
    if (new_items == NULL) {
        free(buffer);
        return 0;
    }

    list->items = new_items;
    list->items[list->count] = buffer;
    list->count++;
    return 1;
}

int split_sql_statements(const char *sql_text, SqlStatementList *list)
{
    const char *segment_start;
    const char *cursor;
    int in_single_quote;

    if (sql_text == NULL || list == NULL) {
        return 0;
    }

    list->items = NULL;
    list->count = 0;
    segment_start = sql_text;
    in_single_quote = 0;

    /*
     * 과제 범위에서는 작은 SQL 문법만 다루므로, 우선 단일 따옴표 문자열만 추적한다.
     * 이렇게 하면 값 내부에 쉼표나 세미콜론이 있어도 문장 분리가 깨지지 않는다.
     */
    for (cursor = sql_text; *cursor != '\0'; cursor++) {
        if (*cursor == '\'') {
            in_single_quote = !in_single_quote;
            continue;
        }

        if (*cursor == ';' && !in_single_quote) {
            if (!append_statement(list, segment_start, (size_t)(cursor - segment_start))) {
                free_sql_statement_list(list);
                return 0;
            }
            segment_start = cursor + 1;
        }
    }

    if (!append_statement(list, segment_start, strlen(segment_start))) {
        free_sql_statement_list(list);
        return 0;
    }

    return 1;
}

void free_sql_statement_list(SqlStatementList *list)
{
    if (list == NULL) {
        return;
    }

    free_string_array(list->items, list->count);
    list->items = NULL;
    list->count = 0;
}
