#ifndef EXECUTOR_H
#define EXECUTOR_H

#include "ast.h"

/*
 * 파싱된 SQL 문장을 실행 단계로 넘긴다.
 * 이번 단계에서는 실행 엔진을 붙이기 위한 호출 지점만 고정한다.
 */
int execute_sql_statement(const SqlStatement *statement);

#endif
