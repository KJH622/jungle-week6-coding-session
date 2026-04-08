#ifndef DOMAIN_PARSER_H
#define DOMAIN_PARSER_H

#include "types.h"
#include "../shared/strvec.h"

int split_statements(const char *sql, StrVec *out);
int is_insert_stmt(const char *stmt);
int is_select_stmt(const char *stmt);

int parse_insert(const char *stmt, const SchemaRegistry *reg, InsertPlan *plan,
                 const char **err_msg);
int parse_select(const char *stmt, const SchemaRegistry *reg, SelectPlan *plan,
                 const char **err_msg);

#endif
