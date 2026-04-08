#include "parser.h"

#include <ctype.h>
#include <string.h>
#include <strings.h>

/* 문자열의 앞뒤 공백을 제거합니다.
   예: "  hello  " -> "hello" */
static void trim_in_place(char *text) {
    char *start = text;
    char *end;

    while (*start != '\0' && isspace((unsigned char)*start)) {
        start++;
    }

    if (start != text) {
        memmove(text, start, strlen(start) + 1);
    }

    end = text + strlen(text);
    while (end > text && isspace((unsigned char)end[-1])) {
        end--;
    }
    *end = '\0';
}

/* 공백을 건너뛰고 첫 번째 실제 문자 위치를 돌려줍니다. */
static const char *skip_spaces(const char *text) {
    while (*text != '\0' && isspace((unsigned char)*text)) {
        text++;
    }
    return text;
}

/* users, age, products 같은 이름에 허용할 문자인지 검사합니다. */
static int is_identifier_char(int ch) {
    return isalnum((unsigned char)ch) || ch == '_';
}

/* 대소문자를 무시하고 키워드를 비교합니다.
   추가 검사로 SELECT가 더 긴 단어 안에서 잘못 매칭되지 않게 막습니다. */
static int keyword_matches(const char *text, const char *keyword) {
    size_t len = strlen(keyword);

    return strncasecmp(text, keyword, len) == 0 &&
           !is_identifier_char((unsigned char)text[len]);
}

/* 현재 위치가 원하는 키워드면 그 뒤로 커서를 이동합니다. */
static int consume_keyword(const char **cursor, const char *keyword) {
    const char *text = skip_spaces(*cursor);
    size_t len = strlen(keyword);

    if (!keyword_matches(text, keyword)) {
        return 0;
    }

    *cursor = text + len;
    return 1;
}

/* 테이블 이름이나 컬럼 이름 같은 식별자 1개를 읽습니다. */
static int parse_identifier(const char **cursor, char *out, size_t out_size) {
    const char *text = skip_spaces(*cursor);
    size_t len = 0;

    if (!is_identifier_char((unsigned char)*text)) {
        return -1;
    }

    while (is_identifier_char((unsigned char)text[len])) {
        len++;
    }

    if (len >= out_size) {
        return -1;
    }

    memcpy(out, text, len);
    out[len] = '\0';
    *cursor = text + len;
    return 0;
}

/* 괄호 안의 쉼표 구분 목록을 읽습니다.
   예: (name, age, major) */
static int parse_parenthesized_identifiers(const char **cursor,
                                           char items[][MAX_NAME_LEN],
                                           int *count) {
    const char *text = skip_spaces(*cursor);

    *count = 0;
    if (*text != '(') {
        return -1;
    }
    text++;

    for (;;) {
        text = skip_spaces(text);
        /* (name, , age)처럼 빈 칸이 있으면 잘못된 문장입니다. */
        if (*text == ')' || *text == '\0') {
            return -1;
        }

        if (*count >= MAX_COLUMNS ||
            parse_identifier(&text, items[*count], sizeof(items[*count])) != 0) {
            return -1;
        }
        (*count)++;

        text = skip_spaces(text);
        if (*text == ',') {
            text++;
            continue;
        }
        /* 닫는 괄호를 만나면 목록 읽기가 끝난 것입니다. */
        if (*text == ')') {
            text++;
            *cursor = text;
            return 0;
        }
        return -1;
    }
}

/* VALUES 목록에서 값 1개를 읽습니다.
   작은따옴표 문자열과 일반 숫자 둘 다 처리합니다. */
static int parse_value_token(const char **cursor, char *out, size_t out_size) {
    const char *text = skip_spaces(*cursor);

    /* 작은따옴표 안의 값은 공백이나 쉼표를 포함할 수 있으므로
       닫는 따옴표가 나올 때까지 통째로 읽습니다. */
    if (*text == '\'') {
        const char *end = strchr(text + 1, '\'');
        size_t len;

        if (end == NULL) {
            return -1;
        }

        len = (size_t)(end - (text + 1));
        if (len >= out_size) {
            return -1;
        }

        memcpy(out, text + 1, len);
        out[len] = '\0';
        *cursor = end + 1;
        return 0;
    }

    {
        const char *end = text;
        char token[MAX_VALUE_LEN];
        size_t len;

        /* 따옴표가 없는 값은 쉼표나 닫는 괄호 전까지만 읽습니다. */
        while (*end != '\0' && *end != ',' && *end != ')') {
            end++;
        }

        len = (size_t)(end - text);
        if (len >= sizeof(token)) {
            return -1;
        }

        memcpy(token, text, len);
        token[len] = '\0';
        trim_in_place(token);

        if (token[0] == '\0' || strlen(token) >= out_size) {
            return -1;
        }

        strcpy(out, token);
        *cursor = end;
        return 0;
    }
}

/* VALUES (...) 전체를 읽고 값들을 각각 분리합니다. */
static int parse_values_clause(const char **cursor,
                               char values[][MAX_VALUE_LEN],
                               int *value_count) {
    const char *text = skip_spaces(*cursor);

    *value_count = 0;
    if (*text != '(') {
        return -1;
    }
    text++;

    for (;;) {
        text = skip_spaces(text);
        /* VALUES()처럼 안이 비어 있으면 이 프로젝트에서는 허용하지 않습니다. */
        if (*text == ')' || *text == '\0') {
            return -1;
        }

        if (*value_count >= MAX_VALUES ||
            parse_value_token(&text, values[*value_count], sizeof(values[*value_count])) != 0) {
            return -1;
        }
        (*value_count)++;

        text = skip_spaces(text);
        if (*text == ',') {
            text++;
            continue;
        }
        if (*text == ')') {
            text++;
            *cursor = text;
            return 0;
        }
        return -1;
    }
}

/* 긴 문장 안에서 FROM 같은 키워드 위치를 찾습니다.
   덕분에 "SELECT 컬럼들 FROM 테이블"을 앞/뒤 두 부분으로 나눌 수 있습니다. */
static const char *find_keyword_position(const char *text, const char *keyword) {
    const char *cursor = text;

    while (*cursor != '\0') {
        if ((cursor == text || !is_identifier_char((unsigned char)cursor[-1])) &&
            keyword_matches(cursor, keyword)) {
            return cursor;
        }
        cursor++;
    }

    return NULL;
}

/* SELECT의 컬럼 부분을 읽습니다.
   "*"도 되고, "name, age" 같은 목록도 됩니다. */
static int parse_select_columns_text(const char *text, Command *cmd) {
    const char *cursor = text;

    cmd->column_count = 0;
    cmd->is_select_all = 0;

    if (strcmp(text, "*") == 0) {
        cmd->is_select_all = 1;
        return 0;
    }

    while (*cursor != '\0') {
        const char *start = cursor;
        const char *end = strchr(cursor, ',');
        char token[MAX_NAME_LEN];
        size_t len;
        size_t i;

        if (cmd->column_count >= MAX_COLUMNS) {
            return -1;
        }

        if (end == NULL) {
            end = cursor + strlen(cursor);
        }

        len = (size_t)(end - start);
        if (len >= sizeof(token)) {
            return -1;
        }

        memcpy(token, start, len);
        token[len] = '\0';
        trim_in_place(token);
        if (token[0] == '\0') {
            return -1;
        }

        /* 이상한 문자가 섞인 컬럼 이름은 허용하지 않습니다. */
        for (i = 0; token[i] != '\0'; i++) {
            if (!is_identifier_char((unsigned char)token[i])) {
                return -1;
            }
        }

        strcpy(cmd->columns[cmd->column_count], token);
        cmd->column_count++;

        if (*end == '\0') {
            break;
        }
        cursor = end + 1;
    }

    return cmd->column_count > 0 ? 0 : -1;
}

/* INSERT 문장을 읽어서 Command 구조체에 채웁니다. */
static int parse_insert(const char *sql, Command *cmd) {
    const char *cursor = sql;

    if (!consume_keyword(&cursor, "INSERT")) {
        return -1;
    }
    if (!consume_keyword(&cursor, "INTO")) {
        return -1;
    }
    if (parse_identifier(&cursor, cmd->table_name, sizeof(cmd->table_name)) != 0) {
        return -1;
    }
    if (parse_parenthesized_identifiers(&cursor, cmd->insert_columns, &cmd->insert_column_count) != 0) {
        return -1;
    }
    if (!consume_keyword(&cursor, "VALUES")) {
        return -1;
    }
    if (parse_values_clause(&cursor, cmd->values, &cmd->value_count) != 0) {
        return -1;
    }

    cursor = skip_spaces(cursor);
    if (*cursor == ';') {
        cursor++;
    }
    cursor = skip_spaces(cursor);
    return *cursor == '\0' ? 0 : -1;
}

/* SELECT 문장을 읽어서 Command 구조체에 채웁니다. */
static int parse_select(const char *sql, Command *cmd) {
    const char *cursor = sql;
    const char *from_pos;
    char columns_text[1024];
    size_t columns_len;

    if (!consume_keyword(&cursor, "SELECT")) {
        return -1;
    }

    cursor = skip_spaces(cursor);
    from_pos = find_keyword_position(cursor, "FROM");
    if (from_pos == NULL || from_pos == cursor) {
        return -1;
    }

    /* FROM 앞부분은 요청한 컬럼 목록입니다. */
    columns_len = (size_t)(from_pos - cursor);
    if (columns_len >= sizeof(columns_text)) {
        return -1;
    }

    memcpy(columns_text, cursor, columns_len);
    columns_text[columns_len] = '\0';
    trim_in_place(columns_text);
    if (parse_select_columns_text(columns_text, cmd) != 0) {
        return -1;
    }

    cursor = from_pos;
    if (!consume_keyword(&cursor, "FROM")) {
        return -1;
    }
    if (parse_identifier(&cursor, cmd->table_name, sizeof(cmd->table_name)) != 0) {
        return -1;
    }

    cursor = skip_spaces(cursor);
    if (*cursor == ';') {
        cursor++;
    }
    cursor = skip_spaces(cursor);
    return *cursor == '\0' ? 0 : -1;
}

/* 파서의 시작점입니다.
   먼저 입력 문자열을 정리한 뒤 INSERT 또는 SELECT 파서로 보냅니다. */
int parse_sql(const char *sql, Command *cmd) {
    char working[8192];
    char *end;

    memset(cmd, 0, sizeof(*cmd));
    cmd->type = CMD_NONE;

    if (strlen(sql) >= sizeof(working)) {
        return -1;
    }

    strcpy(working, sql);
    trim_in_place(working);

    /* main.c에서 문장 분리는 이미 했지만,
       여기서 세미콜론을 한 번 더 정리하면 파서가 조금 더 안전해집니다. */
    end = working + strlen(working);
    while (end > working && end[-1] == ';') {
        end--;
    }
    *end = '\0';
    trim_in_place(working);

    if (working[0] == '\0') {
        return 0;
    }

    /* 문장 시작 키워드를 보고 어떤 파서가 처리할지 결정합니다. */
    if (keyword_matches(working, "INSERT")) {
        cmd->type = CMD_INSERT;
        return parse_insert(working, cmd);
    }

    if (keyword_matches(working, "SELECT")) {
        cmd->type = CMD_SELECT;
        return parse_select(working, cmd);
    }

    return -1;
}
