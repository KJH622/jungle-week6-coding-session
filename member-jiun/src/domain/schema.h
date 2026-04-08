#ifndef DOMAIN_SCHEMA_H
#define DOMAIN_SCHEMA_H

#include "types.h"

int load_required_schemas(SchemaRegistry *reg);
const TableSchema *find_table(const SchemaRegistry *reg, const char *name);
int find_col_index(const TableSchema *schema, const char *col_name);

#endif
