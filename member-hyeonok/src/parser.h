#ifndef PARSER_H
#define PARSER_H

#include "ast.h"

typedef enum ParseResult {
    PARSE_SUCCESS = 0,
    PARSE_INVALID_QUERY = 1,
    PARSE_INTERNAL_ERROR = 2
} ParseResult;

/*
 * SQL 한 문장의 종류를 판별한 뒤, 적절한 개별 파서를 호출한다.
 * 성공 시 executor 가 바로 사용할 수 있는 AST 를 채워서 반환한다.
 */
ParseResult parse_query(const char *sql_text, SqlStatement *statement);

/*
 * 기존 bool 형태 인터페이스가 필요한 호출부를 위해 제공하는 래퍼다.
 * 성공이면 1, 실패면 0을 반환한다.
 */
int parse_sql_statement(const char *sql_text, SqlStatement *statement);

/*
 * 파서가 채운 SQL 문장 구조체 내부 메모리를 해제한다.
 */
void free_sql_statement(SqlStatement *statement);

#endif
