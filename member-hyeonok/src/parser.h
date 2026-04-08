#ifndef PARSER_H
#define PARSER_H

#include "ast.h"

/*
 * 하나의 SQL 문장 문자열을 파싱 결과 구조체로 옮긴다.
 * 이번 단계에서는 raw SQL 보관과 인터페이스 고정에 집중한다.
 */
int parse_sql_statement(const char *sql_text, SqlStatement *statement);

/*
 * 파서가 채운 SQL 문장 구조체 내부 메모리를 해제한다.
 */
void free_sql_statement(SqlStatement *statement);

#endif
