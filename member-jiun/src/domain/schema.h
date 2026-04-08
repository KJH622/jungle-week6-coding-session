#ifndef DOMAIN_SCHEMA_H
#define DOMAIN_SCHEMA_H

#include "types.h"
#include "../shared/strvec.h"

int load_required_schemas(SchemaRegistry *reg);
int load_named_schema(const char *table_name, TableSchema *out);
int list_schema_tables(StrVec *out);
void free_table_schema(TableSchema *schema);
void free_schema_registry(SchemaRegistry *reg);
const TableSchema *find_table(const SchemaRegistry *reg, const char *name);
int find_col_index(const TableSchema *schema, const char *col_name);

#endif
