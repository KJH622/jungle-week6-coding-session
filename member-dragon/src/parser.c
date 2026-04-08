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

/* 작은따옴표 문자열을 읽습니다.
   SQL의 escaped quote(`''`)는 실제 값 안의 작은따옴표 1개로 바꿔 저장합니다. */
static int parse_quoted_value(const char **cursor, char *out, size_t out_size) {
    const char *text = *cursor;
    size_t len = 0;

    if (*text != '\'') {
        return -1;
    }
    text++;

    while (*text != '\0') {
        if (*text == '\'') {
            /* ''는 문자열 종료가 아니라 값 안의 작은따옴표입니다. */
            if (text[1] == '\'') {
                if (len + 1 >= out_size) {
                    return -1;
                }
                out[len++] = '\'';
                text += 2;
                continue;
            }

            out[len] = '\0';
            *cursor = text + 1;
            return 0;
        }

        if (len + 1 >= out_size) {
            return -1;
        }

        out[len++] = *text;
        text++;
    }

    return -1;
}

/* VALUES/WHERE/SET에서 값 1개를 읽습니다.
   quoted string은 escaped quote까지 처리하고,
   unquoted value는 공백이나 다음 구분자를 만나면 끝난 것으로 봅니다. */
static int parse_value_token(const char **cursor, char *out, size_t out_size) {
    const char *text = skip_spaces(*cursor);

    if (*text == '\'') {
        if (parse_quoted_value(&text, out, out_size) != 0) {
            return -1;
        }
        *cursor = text;
        return 0;
    }

    {
        const char *end = text;
        char token[MAX_VALUE_LEN];
        size_t len;

        /* 따옴표가 없는 값은 공백, 쉼표, 닫는 괄호, 세미콜론 전까지만 읽습니다.
           이렇게 해야 WHERE age > 22 ORDER BY ... 에서
           22 뒤의 ORDER BY가 값 안으로 잘못 들어가지 않습니다. */
        while (*end != '\0' &&
               !isspace((unsigned char)*end) &&
               *end != ',' &&
               *end != ')' &&
               *end != ';') {
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

/* WHERE 절의 비교 연산자를 읽습니다.
   길이가 2인 연산자부터 먼저 확인해야 >=, <=, !=를 제대로 읽을 수 있습니다. */
static int parse_compare_op(const char **cursor, CompareOp *op) {
    const char *text = skip_spaces(*cursor);

    /* >=를 >와 = 두 개로 잘못 나누지 않으려면
       길이가 긴 연산자를 먼저 검사해야 합니다. */
    if (strncmp(text, "!=", 2) == 0) {
        *op = OP_NE;
        *cursor = text + 2;
        return 0;
    }
    if (strncmp(text, ">=", 2) == 0) {
        *op = OP_GE;
        *cursor = text + 2;
        return 0;
    }
    if (strncmp(text, "<=", 2) == 0) {
        *op = OP_LE;
        *cursor = text + 2;
        return 0;
    }
    if (*text == '=') {
        *op = OP_EQ;
        *cursor = text + 1;
        return 0;
    }
    if (*text == '>') {
        *op = OP_GT;
        *cursor = text + 1;
        return 0;
    }
    if (*text == '<') {
        *op = OP_LT;
        *cursor = text + 1;
        return 0;
    }

    return -1;
}

/* WHERE <column> <operator> <value> 형태를 읽습니다.
   WHERE가 없으면 0, 있으면 1, 문법이 틀리면 -1을 반환합니다. */
static int parse_where_clause(const char **cursor, WhereCondition *where) {
    const char *text = *cursor;

    /* WHERE는 선택 기능이라 없다고 바로 에러를 내지 않습니다.
       "없음"과 "문법 오류"를 구분하려고 0/1/-1을 나눠 씁니다. */
    if (!consume_keyword(&text, "WHERE")) {
        return 0;
    }

    /* WHERE age >= 20 에서 age 부분을 읽습니다. */
    if (parse_identifier(&text, where->column, sizeof(where->column)) != 0) {
        return -1;
    }

    /* =, !=, >, <, >=, <= 중 하나를 읽습니다. */
    if (parse_compare_op(&text, &where->op) != 0) {
        return -1;
    }

    /* 비교값은 숫자일 수도 있고, 따옴표 문자열일 수도 있습니다. */
    if (parse_value_token(&text, where->value, sizeof(where->value)) != 0) {
        return -1;
    }

    *cursor = text;
    return 1;
}

/* ORDER BY <column> [ASC|DESC] 형태를 읽습니다.
   방향이 생략되면 기본값은 ASC입니다. */
static int parse_order_by_clause(const char **cursor, OrderByClause *order) {
    const char *text = *cursor;

    /* ORDER BY도 선택 기능이므로 없으면 0을 반환합니다. */
    if (!consume_keyword(&text, "ORDER")) {
        return 0;
    }
    if (!consume_keyword(&text, "BY")) {
        return -1;
    }
    if (parse_identifier(&text, order->column, sizeof(order->column)) != 0) {
        return -1;
    }

    /* 방향을 안 쓴 쿼리도 지원하므로 기본값을 먼저 ASC로 넣어 둡니다. */
    order->direction = ORDER_ASC;
    text = skip_spaces(text);
    if (keyword_matches(text, "DESC")) {
        text += 4;
        order->direction = ORDER_DESC;
    } else if (keyword_matches(text, "ASC")) {
        text += 3;
    }

    *cursor = text;
    return 1;
}

/* LIMIT <정수> 형태를 읽습니다. */
static int parse_limit_clause(const char **cursor, int *limit_count) {
    const char *text = *cursor;

    /* LIMIT이 없으면 SELECT 전체 결과를 그대로 사용합니다. */
    if (!consume_keyword(&text, "LIMIT")) {
        return 0;
    }

    text = skip_spaces(text);
    *limit_count = 0;

    if (!isdigit((unsigned char)*text)) {
        return -1;
    }

    /* 숫자를 글자 하나씩 읽어 정수값으로 만듭니다. */
    while (isdigit((unsigned char)*text)) {
        *limit_count = (*limit_count * 10) + (*text - '0');
        text++;
    }

    *cursor = text;
    return 1;
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

/* DELETE FROM <table> [WHERE ...] 문장을 읽어서 Command 구조체에 채웁니다. */
static int parse_delete(const char *sql, Command *cmd) {
    const char *cursor = sql;
    int where_result;

    /* DELETE는 문법이 짧지만, WHERE가 있으면 뒤에 조건까지 같이 읽어야 합니다. */
    if (!consume_keyword(&cursor, "DELETE")) {
        return -1;
    }
    if (!consume_keyword(&cursor, "FROM")) {
        return -1;
    }
    if (parse_identifier(&cursor, cmd->table_name, sizeof(cmd->table_name)) != 0) {
        return -1;
    }

    /* WHERE가 있으면 cmd->where에 조건이 채워지고,
       없으면 has_where만 0으로 남습니다. */
    where_result = parse_where_clause(&cursor, &cmd->where);
    if (where_result < 0) {
        return -1;
    }
    cmd->has_where = (where_result == 1);

    cursor = skip_spaces(cursor);
    if (*cursor == ';') {
        cursor++;
    }
    cursor = skip_spaces(cursor);
    return *cursor == '\0' ? 0 : -1;
}

/* UPDATE의 SET 절 목록을 읽습니다.
   예: SET age = 20, major = '수학' */
static int parse_set_clause_list(const char **cursor, Command *cmd) {
    const char *text = *cursor;

    cmd->set_clause_count = 0;

    for (;;) {
        SetClause *clause;

        /* UPDATE는 여러 컬럼을 한 번에 바꿀 수 있지만,
           고정 크기 배열을 넘으면 더 담을 수 없습니다. */
        if (cmd->set_clause_count >= MAX_SET_CLAUSES) {
            return -1;
        }

        clause = &cmd->set_clauses[cmd->set_clause_count];
        /* SET age = 20 에서 age 부분을 읽습니다. */
        if (parse_identifier(&text, clause->column, sizeof(clause->column)) != 0) {
            return -1;
        }

        text = skip_spaces(text);
        if (*text != '=') {
            return -1;
        }
        text++;

        /* = 오른쪽 값은 숫자 또는 문자열일 수 있습니다. */
        if (parse_value_token(&text, clause->value, sizeof(clause->value)) != 0) {
            return -1;
        }

        cmd->set_clause_count++;

        text = skip_spaces(text);
        /* 쉼표가 나오면 아직 SET 목록이 더 있다는 뜻입니다. */
        if (*text == ',') {
            text++;
            continue;
        }
        break;
    }

    *cursor = text;
    return cmd->set_clause_count > 0 ? 0 : -1;
}

/* UPDATE <table> SET ... [WHERE ...] 문장을 읽어서 Command 구조체에 채웁니다. */
static int parse_update(const char *sql, Command *cmd) {
    const char *cursor = sql;
    int where_result;

    /* UPDATE는 테이블 이름 -> SET 목록 -> 선택적 WHERE 순서로 읽습니다. */
    if (!consume_keyword(&cursor, "UPDATE")) {
        return -1;
    }
    if (parse_identifier(&cursor, cmd->table_name, sizeof(cmd->table_name)) != 0) {
        return -1;
    }
    if (!consume_keyword(&cursor, "SET")) {
        return -1;
    }
    if (parse_set_clause_list(&cursor, cmd) != 0) {
        return -1;
    }

    /* WHERE가 없으면 모든 row를 바꾸는 UPDATE가 됩니다. */
    where_result = parse_where_clause(&cursor, &cmd->where);
    if (where_result < 0) {
        return -1;
    }
    cmd->has_where = (where_result == 1);

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
    int where_result;
    int order_result;
    int limit_result;

    if (!consume_keyword(&cursor, "SELECT")) {
        return -1;
    }

    /* SELECT 다음에는 컬럼 목록이 오고,
       그 뒤에서 FROM 키워드를 찾아 앞/뒤를 나눕니다. */
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

    /* SELECT의 확장 기능은 순서대로 WHERE -> ORDER BY -> LIMIT만 허용합니다. */
    where_result = parse_where_clause(&cursor, &cmd->where);
    if (where_result < 0) {
        return -1;
    }
    cmd->has_where = (where_result == 1);

    order_result = parse_order_by_clause(&cursor, &cmd->order_by);
    if (order_result < 0) {
        return -1;
    }
    cmd->has_order_by = (order_result == 1);

    limit_result = parse_limit_clause(&cursor, &cmd->limit_count);
    if (limit_result < 0) {
        return -1;
    }
    cmd->has_limit = (limit_result == 1);

    /* 모든 선택 절을 읽은 뒤 남은 글자가 있으면
       이 프로젝트가 모르는 문법이 뒤에 붙은 것으로 봅니다. */
    cursor = skip_spaces(cursor);
    if (*cursor == ';') {
        cursor++;
    }
    cursor = skip_spaces(cursor);
    return *cursor == '\0' ? 0 : -1;
}

/* 파서의 시작점입니다.
   먼저 입력 문자열을 정리한 뒤
   문장 시작 키워드에 맞는 세부 파서로 보내는 역할을 합니다. */
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

    /* 문장 시작 키워드를 보고 어떤 파서가 처리할지 결정합니다.
       여기서부터는 각 문장 종류별 전용 함수가 세부 문법을 검사합니다. */
    if (keyword_matches(working, "INSERT")) {
        cmd->type = CMD_INSERT;
        return parse_insert(working, cmd);
    }

    if (keyword_matches(working, "SELECT")) {
        cmd->type = CMD_SELECT;
        return parse_select(working, cmd);
    }

    if (keyword_matches(working, "DELETE")) {
        cmd->type = CMD_DELETE;
        return parse_delete(working, cmd);
    }

    if (keyword_matches(working, "UPDATE")) {
        cmd->type = CMD_UPDATE;
        return parse_update(working, cmd);
    }

    return -1;
}
